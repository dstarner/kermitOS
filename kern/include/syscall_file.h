/*
*  This file holds all of the sys_call function definitions
*  that pertain to the filesystem and files.
*
*  Note that a file struct is defined in <file.h>
*/

#ifndef SYSCALL_FILE_H
#define SYSCALL_FILE_H

struct f_handler {

    // Lock for logistics
    struct rwlock *fh_lock;

    // Vnode for where memory is.
    struct vnode *fh_vnode;

    // Reference count for what is using file
    unsigned int ref_count;

    // File permissions
    mode_t fh_perms;

    // Current file position/offset
    off_t fh_position;
};

/*
* DESCRIPTION
* write writes up to buflen bytes to the file specified by fd, at the location
* in the file specified by the current seek position of the file, taking the
* data from the space pointed to by buf. The file must be open for writing.
*
*The current seek position of the file is advanced by the number of bytes
* written.
* Each write (or read) operation is atomic relative to other I/O to the same
* file. Note that the kernel is not obliged to (and generally cannot) make the
* write atomic with respect to other threads in the same process accessing the
* I/O buffer during the write.
*
* From OS161 man pages.

* RETURN
* The count of bytes written is returned. This count should be positive. A
* return value of 0 means that nothing could be written, but that no error
* occurred; this only occurs at end-of-file on fixed-size objects. On error,
* write returns -1 and sets errno to a suitable error code for the error
* condition encountered.
*
* ERRORS
* The following error codes should be returned under the conditions given.
* Other error codes may be returned for other cases not mentioned here.
*
*   EBADF	fd is not a valid file descriptor, or was not opened for writing.
*   EFAULT	Part or all of the address space pointed to by buf is invalid.
*   ENOSPC	There is no free space remaining on the filesystem containing the file.
*   EIO	A hardware I/O error occurred writing the data.
*/
ssize_t
write(int fd, const void *buf, size_t buflen, int * err);

/*
* DESCRIPTION
* read reads up to buflen bytes from the file specified by fd, at the location
* in the file specified by the current seek position of the file, and stores
* them in the space pointed to by buf. The file must be open for reading.

* The current seek position of the file is advanced by the number of bytes read.

* Each read (or write) operation is atomic relative to other I/O to the same
* file. Note that the kernel is not obliged to (and generally cannot) make the
* read atomic with respect to other threads in the same process accessing the
* I/O buffer during the read.
*
* RETURN
* The count of bytes read is returned. This count should be positive. A return
* value of 0 should be construed as signifying end-of-file. On error, read
* returns -1 and sets errno to a suitable error code for the error condition
* encountered.
*
* Note that in some cases, particularly on devices, fewer than buflen (but
* greater than zero) bytes may be returned. This depends on circumstances and
* does not necessarily signify end-of-file.
*
* ERRORS
* The following error codes should be returned under the conditions given.
* Other error codes may be returned for other cases not mentioned here.
*   EBADF	fd is not a valid file descriptor, or was not opened for reading.
*   EFAULT	Part or all of the address space pointed to by buf is invalid.
*   EIO	    A hardware I/O error occurred reading the data.
*/
ssize_t
read(int fd, void *buf, size_t buflen, int * err);

#endif // SYSCALL_FILE_H
