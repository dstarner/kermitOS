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
  if (fd < 0 || fd > __OPEN_MAX) {
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

  struct uio * writer_uio;
  struct iovec * writer_iovec;

  // Length and buffer
  writer_iovec->iov_ubase = buf;
  writer_iovec->iov_len = buflen;

  // Set up uio
  writer_uio->uio_iov = writer_iovec;
  writer_uio->uio_iovcnt = 1;

  writer_uio->uio_rw = UIO_WRITE;  // Set up for writing
  writer_uio->uio_segflg = UIO_USERSPACE;
  writer_uio->uio_resid = buflen;
  writer_uio->uio_resid = buflen;
  writer_uio->uio_offset = curproc->f_table[fd]->fh_position;
  writer_uio->uio_space = curproc->p_addrspace;

};

ssize_t
sys_read(int fd, void *buf, size_t buflen, int * err) {

  // Check if valid index
  if (fd < 0 || fd > __OPEN_MAX) {
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

  struct uio * reader_uio;
  struct iovec * reader_iovec;

  // Length and buffer
  reader_iovec->iov_ubase = buf;
  reader_iovec->iov_len = buflen;

  // Set up uio
  reader_uio->uio_iov = reader_iovec;
  reader_uio->uio_iovcnt = 1;

  reader_uio->uio_rw = UIO_READ;  // Set up for reading
  reader_uio->uio_segflg = UIO_USERSPACE;
  reader_uio->uio_resid = buflen;
  reader_uio->uio_offset = curproc->f_table[fd]->fh_position;
  reader_uio->uio_space = curproc->p_addrspace;
  reader_uio->uio_resid = buflen;

  // Start reading
  rwlock_acquire_read(curproc->f_table[fd]->fh_lock);

  // The amount remaining
  int remaining = buflen;

  // Read
  int result = VOP_READ(curproc->f_table[fd]->fh_vnode, reader_uio);

  // Amount transfered
  remaining -= reader_uio->uio_resid;

  // End Reading
  rwlock_release_read(curproc->f_table[fd]->fh_lock);

  if (result) {
    *err = result;
    return -1;
  } else {
    return remaining;
  }
  
};
