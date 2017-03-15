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

/* Initialize file table with stdin/out/err */
void init_std() {
  for (int fd = 0; fd < 3; fd++) {

    int failure = 0;

    char con[] = "con:";

    // Create basic
    curproc->f_table[fd] = kmalloc(sizeof(struct f_handler));
    if (curproc->f_table[fd] == NULL) {
      return;
    }
    curproc->f_table[fd]->fh_position = 0;
    curproc->f_table[fd]->ref_count = 1;

    curproc->f_table[fd]->fh_lock = lock_create("stdcon");
    if (curproc->f_table[fd]->fh_lock == NULL) {
      failure = ENOMEM;
    }

    if (failure) {
      lock_destroy(curproc->f_table[fd]->fh_lock);
      vfs_close(curproc->f_table[fd]->fh_vnode);
      kfree(curproc->f_table[fd]);
      return;
    }

    // stdin
    if (fd == 0) {
      curproc->f_table[fd]->fh_perms = O_RDONLY;
      failure = vfs_open(con, O_RDONLY, 0664, &(curproc->f_table[fd]->fh_vnode));

    // stdout
    } else if (fd == 1) {
      curproc->f_table[fd]->fh_perms = O_WRONLY;
      failure = vfs_open(con, O_WRONLY, 0664, &(curproc->f_table[fd]->fh_vnode));

    } else {
      curproc->f_table[fd]->fh_perms = O_WRONLY;
      failure = vfs_open(con, O_WRONLY, 0664, &(curproc->f_table[fd]->fh_vnode));
    }

    if (failure) {
      // Delete all made con:'s if bad.
      for (int i=0; i <= fd; i++) {
        lock_destroy(curproc->f_table[i]->fh_lock);
        vfs_close(curproc->f_table[i]->fh_vnode);
        kfree(curproc->f_table[i]);
      }
      return;
    }

  }

}

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
  lock_acquire(curproc->f_table[fd]->fh_lock);

  int remaining = buflen;

  writer_uio.uio_offset = curproc->f_table[fd]->fh_position;

  int result = VOP_WRITE(curproc->f_table[fd]->fh_vnode, &writer_uio);

  remaining -= writer_uio.uio_resid;

  // Update offset
  curproc->f_table[fd]->fh_position = writer_uio.uio_offset;

  // Stop writing
  lock_release(curproc->f_table[fd]->fh_lock);

  if (result) {*err = result; return -1;}

  return remaining;

};

ssize_t sys_read(int fd, void *buf, size_t buflen, int * err) {

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
  lock_acquire(curproc->f_table[fd]->fh_lock);

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
  lock_release(curproc->f_table[fd]->fh_lock);

  if (result) {
    *err = result;
    return -1;
  } else {
    return remaining;
  };
};

int sys__getcwd(char *buf, size_t buflen, int *err) {

  if (buf == NULL) {
    *err = EFAULT;
    return -1;
  }

  if (buflen <= 0) {
    *err = EINVAL;
    return -1;
  }

  struct uio getcwd_uio;
  struct iovec getcwd_iovec;
  int remaining = 0, failure = 0;

  getcwd_iovec.iov_ubase = (void *)buf;
  getcwd_iovec.iov_len = buflen;

  getcwd_uio.uio_iovcnt = 1;
  getcwd_uio.uio_iov = &getcwd_iovec;
  getcwd_uio.uio_segflg = UIO_USERSPACE;
  getcwd_uio.uio_rw = UIO_READ;
  getcwd_uio.uio_space = curproc->p_addrspace;
  getcwd_uio.uio_resid = buflen;
  remaining = getcwd_uio.uio_resid;

  failure = vfs_getcwd(&getcwd_uio);

  if (failure) {*err = failure; return -1;}

  return remaining - getcwd_uio.uio_resid;

}

int sys_open(const char *f_name, int flags, mode_t mode, int *err) {
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

  size_t length;

  // Some more setup here i think?
  int failure = copyinstr((const_userptr_t) f_name, filename, PATH_MAX, &length);

  if (failure) {
    kfree(filename);
    *err = EFAULT;
    return -1;
  }

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

  curproc->f_table[pos_fd]->fh_lock = lock_create("file_handler");
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
    lock_destroy(curproc->f_table[pos_fd]->fh_lock);
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
  lock_acquire(curproc->f_table[fd]->fh_lock);

  // Reduce the number of threads using it.
  curproc->f_table[fd]->ref_count--;

  // If nothing else is using it.
  if (curproc->f_table[fd]->ref_count == 0) {
    // Clean up and close the vnode
    vfs_close(curproc->f_table[fd]->fh_vnode);
    // Release and destroy the lock
    lock_release(curproc->f_table[fd]->fh_lock);
    lock_destroy(curproc->f_table[fd]->fh_lock);

    // Free and NULL
    kfree(curproc->f_table[fd]);
    curproc->f_table[fd] = NULL;

  } else {
    // Just release and move on
    lock_release(curproc->f_table[fd]->fh_lock);
  }

  return 0;
}

off_t sys_lseek(int fd, off_t pos, int whence, int * err) {
  // Bad fd
  if (fd < 0 || fd > OPEN_MAX) {
    *err = EBADF;
    return -1;
  }

  if (curproc->f_table[fd] == NULL) {
    *err = EBADF;
    return -1;
  }

  // Bad whence
  if (!(whence == SEEK_SET || whence == SEEK_END || whence == SEEK_CUR)) {
    *err = EINVAL;
    return -1;
  }


  lock_acquire(curproc->f_table[fd]->fh_lock);

  // Seeking on a console
  if (!VOP_ISSEEKABLE(curproc->f_table[fd]->fh_vnode)) {
    *err = ESPIPE;
    return -1;
  }

  // Easy to remember current offset
  off_t cur_pos = curproc->f_table[fd]->fh_position;

  // Get end of file
  struct stat stats;
  int failure = VOP_STAT(curproc->f_table[fd]->fh_vnode, &stats);

  if (failure) {
    *err = EINVAL;
    return -1;
  }

  // The total size of the file
  off_t file_size = stats.st_size;

  // Check if valid new position
  if (whence == SEEK_SET && pos < 0) {
    *err = EINVAL;
    return -1;
  }


  if (whence == SEEK_CUR && cur_pos + pos < 0) {
    *err = EINVAL;
    return -1;
  }

  if (whence == SEEK_END && file_size + pos < 0) {
    *err = EINVAL;
    return -1;
  }

  // Update position
  if (whence == SEEK_END) {
    // Go to end of file
    curproc->f_table[fd]->fh_position = (file_size + pos);
  } else if (whence == SEEK_CUR) {
    // Add to current process
    curproc->f_table[fd]->fh_position = curproc->f_table[fd]->fh_position + pos;
  } else if (whence == SEEK_SET) {
    // Set to pos
    curproc->f_table[fd]->fh_position = pos;
  }

  // Release
  lock_release(curproc->f_table[fd]->fh_lock);

  return curproc->f_table[fd]->fh_position;

}

int sys_chdir(const char * path, int * err) {
  // Pathname for kernel space
  char * pathname = kmalloc(sizeof(char) * PATH_MAX);

  // Size of pathname
  size_t length;

  // User to kernel space
  int response = copyinstr((const_userptr_t) path, pathname, PATH_MAX, &length);

  if (response) {
    kfree(pathname);
    *err = EFAULT;
    return -1;
  }

  // Actually change directory
  response = vfs_chdir(pathname);

  if (response) {
    kfree(pathname);
    // Response will give error
    *err = response;
    return -1;
  }

  kfree(pathname);
  return 0;

}

int dup2(int oldfd, int newfd, int * err) {
  // Make sure the fd is at a valid index.
  if ((oldfd < 0 || oldfd > OPEN_MAX) || (newfd < 0 || newfd > OPEN_MAX)) {
    *err = EBADF;
    return -1;
  }

  // Using dup2 to clone a file handle onto itself has no effect
  if (oldfd == newfd) {
    return 0;
  }


  // If the new fd already exists, close the file first
  int *close_error = 0;
  if (curproc->f_table[newfd] != NULL) {
    sys_close(newfd, close_error);
  }

  // Start Critical section for oldfd
  lock_acquire(curproc->f_table[oldfd]->fh_lock);

  // If the file could not be closed for some reason.
  // If the old fd is empty then it doesn't work either.
  if (close_error || curproc->f_table[oldfd] == NULL) {
    *err = EBADF;
    return -1;
  }

  // Set pointer of newfd to that of oldfd
  curproc->f_table[newfd] = curproc->f_table[oldfd];

  // End critical section
  lock_release(curproc->f_table[oldfd]->fh_lock);

  return 0;

}
