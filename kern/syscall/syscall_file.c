#include <types.h>
#include <syscall.h>
#include <synch.h>
#include <vfs.h>
#include <current.h>
#include <proc.h>
#include <limits.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <uio.h>
#include <kern/iovec.h>
#include <copyinout.h>
#include <vnode.h>
#include <kern/seek.h>
#include <kern/stat.h>


// fd is file descriptor

ssize_t sys_write(int fd, void *buf, size_t buflen, int * err) {

  // Check if valid index
  if (fd < 0 || fd > OPEN_MAX) {
    *err = EBADF;
    return -1;
  }

  // Check if the file exists
  if (curproc->f_table[fd] == NULL) {
    *err = EBADF;
    return -1;
  }

  // Check if not open for writing
  if (!(curproc->f_table[fd]->fh_perms & O_ACCMODE)) {
    *err = EBADF;
    return -1;
  }

  if (buflen == 0) {
    *err = EFAULT;
    return -1;
  }

  struct uio writer_uio;
  struct iovec writer_iovec;

  // Length and buffer
  writer_iovec.iov_ubase = buf;
  writer_iovec.iov_len = buflen;

  // Set up uio
  writer_uio.uio_iov = &writer_iovec;
  writer_uio.uio_iovcnt = 1;

  writer_uio.uio_rw = UIO_WRITE;  // Set up for writing
  writer_uio.uio_segflg = UIO_USERSPACE;
  writer_uio.uio_resid = buflen;
  writer_uio.uio_offset = curproc->f_table[fd]->fh_position;
  writer_uio.uio_space = curproc->p_addrspace;

  // Start writing
  rwlock_acquire_write(curproc->f_table[fd]->fh_lock);

  int remaining = buflen;

  writer_uio.uio_offset = curproc->f_table[fd]->fh_position;

  int result = VOP_WRITE(curproc->f_table[fd]->fh_vnode, &writer_uio);

  remaining -= writer_uio.uio_resid;

  // Update offset
  curproc->f_table[fd]->fh_position = writer_uio.uio_offset;

  // Stop writing
  rwlock_release_write(curproc->f_table[fd]->fh_lock);

  if (result) {
    *err = result;
    return -1;
  } else {
    return remaining;
  }

};

ssize_t
sys_read(int fd, void *buf, size_t buflen, int * err) {

  // Check if valid index
  if (fd < 0 || fd > OPEN_MAX) {
    *err = EBADF;
    return -1;
  }

  // Check if the file exists
  if (curproc->f_table[fd] == NULL) {
    *err = EBADF;
    return -1;
  }

  // Check if not open for reading
  if (curproc->f_table[fd]->fh_perms & O_WRONLY) {
    *err = EBADF;
    return -1;
  }

  if (buflen == 0) {
    *err = EFAULT;
    return -1;
  }

  struct uio reader_uio;
  struct iovec reader_iovec;

  // Length and buffer
  reader_iovec.iov_ubase = buf;
  reader_iovec.iov_len = buflen;

  // Set up uio
  reader_uio.uio_iov = &reader_iovec;
  reader_uio.uio_iovcnt = 1;

  reader_uio.uio_rw = UIO_READ;  // Set up for reading
  reader_uio.uio_segflg = UIO_USERSPACE;
  reader_uio.uio_resid = buflen;
  reader_uio.uio_offset = curproc->f_table[fd]->fh_position;
  reader_uio.uio_space = curproc->p_addrspace;

  // Start reading
  rwlock_acquire_read(curproc->f_table[fd]->fh_lock);

  // The amount remaining
  int remaining = buflen;

  // Current file offset
  reader_uio.uio_offset = curproc->f_table[fd]->fh_position;

  // Read
  int result = VOP_READ(curproc->f_table[fd]->fh_vnode, &reader_uio);

  // Amount transfered
  remaining -= reader_uio.uio_resid;

  // Update offset
  curproc->f_table[fd]->fh_position = reader_uio.uio_offset;

  // End Reading
  rwlock_release_read(curproc->f_table[fd]->fh_lock);

  if (result) {
    *err = result;
    return -1;
  } else {
    return remaining;
  };
};

int open(const char *f_name, int flags, mode_t mode, int *err) {

   // Invalid flags
   if (flags > O_NOCTTY) {
     *err = EINVAL;
     return -1;
   }

   // Invalid name
   if (f_name == NULL) {
     *err = EFAULT;
     return -1;
   }

   // Copy name for file_handle
   char * filename = kmalloc(sizeof(char) * NAME_MAX);

   // Some more setup here i think?

   // Skip 0, 1, 2 for std in/out/err
   int pos_fd = 3;

   // Look for an open fd within range
   while (pos_fd < OPEN_MAX && curproc->f_table[pos_fd] != NULL) {
     pos_fd++;
   }

   // Too many files open
   if (pos_fd >= OPEN_MAX) {
     *err = EMFILE;
     return -1;
   }

   // Create f_handle
   curproc->f_table[pos_fd] = kmalloc(sizeof(struct f_handler));
   if (curproc->f_table[pos_fd] == NULL) {
     *err = ENOMEM;
     // Free name and clear for shits n giggles
     kfree(filename);
     curproc->f_table[pos_fd] = NULL;
     return -1;
   }

   curproc->f_table[pos_fd]->fh_lock = rwlock_create("file_handler");
   if (curproc->f_table[pos_fd]->fh_lock == NULL) {
     *err = ENOMEM;
     kfree(filename);
     kfree(curproc->f_table[pos_fd]->fh_lock);
     curproc->f_table[pos_fd] = NULL;
     return -1;
   }

   curproc->f_table[pos_fd]->ref_count = 1;
   curproc->f_table[pos_fd]->fh_perms = flags;
   curproc->f_table[pos_fd]->fh_position = 0;

   int vnode_fail = vfs_open(filename, flags, mode, &(curproc->f_table[pos_fd]->fh_vnode));

   // If failure, clean up
   if (vnode_fail) {
     *err = vnode_fail;
     kfree(filename);
     rwlock_destroy(curproc->f_table[pos_fd]->fh_lock);
     kfree(curproc->f_table[pos_fd]);
     curproc->f_table[pos_fd] = NULL;
     return -1;
   }

   kfree(filename);

   return pos_fd;

 }

int sys_close(int fd, int *err) {

  // Check valid fd
  if (fd < 0 || fd > OPEN_MAX) {
    *err = EBADF;
    return -1;
  }

  if (curproc->f_table[fd] == NULL) {
    *err = EBADF;
    return -1;
  }

  // Acquire write so we know we are the only ones messing with it.
  rwlock_acquire_write(curproc->f_table[fd]->fh_lock);

  // Reduce the number of threads using it.
  curproc->f_table[fd]->ref_count--;

  // If nothing else is using it.
  if (curproc->f_table[fd]->ref_count == 0) {
    // Clean up and close the vnode
    vfs_close(curproc->f_table[fd]->fh_vnode);
    // Release and destroy the lock
    rwlock_release_write(curproc->f_table[fd]->fh_lock);
    rwlock_destroy(curproc->f_table[fd]->fh_lock);

    // Free and NULL
    kfree(curproc->f_table[fd]);
    curproc->f_table[fd] = NULL;

  } else {
    // Just release and move on
    rwlock_release_write(curproc->f_table[fd]->fh_lock);
  }

  return 0;
}
