/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYSCALL_H_
#define _SYSCALL_H_


#include <cdefs.h> /* for __DEAD */
#include <spl.h>
#include <addrspace.h>
struct trapframe; /* from <machine/trapframe.h> */

struct f_handler {
    struct lock *fh_lock;   // Lock for logistics
    struct vnode *fh_vnode; // Vnode for where memory is.
    unsigned int ref_count; // Reference count for what is using file
    mode_t fh_perms;        // File permissions
    off_t fh_position;      // Current file position/offset

};

/*
 * The system call dispatcher.
 */
void syscall(struct trapframe *tf);

/*
 * Support functions.
 */

/* Helper for fork(). You write this. */
void enter_forked_process(struct trapframe *tf);

/* Enter user mode. Does not return. */
__DEAD void enter_new_process(int argc, userptr_t argv, userptr_t env,
		       vaddr_t stackptr, vaddr_t entrypoint);


/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);
int sys___time(userptr_t user_seconds, userptr_t user_nanoseconds);

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
ssize_t sys_write(int, void *, size_t, int *);

/* Initialize file table with stdin/out/err */
void init_std(void);

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
ssize_t sys_read(int, void *, size_t, int *);

int sys_open(const char*, int, mode_t, int *);

int sys_close(int, int *);

int dup2(int, int, int *);

off_t sys_lseek(int, off_t, int, int*);

int sys_chdir(const char *, int*);

int sys__getcwd(char *, size_t, int*);

/*
*   PROCESS SYSCALLS
*/
pid_t sys_getpid(void);

pid_t sys_waitpid(pid_t, int *, int, int *);

void sys_exit(int, bool);

int sys_fork(struct trapframe*, int*);

void new_thread_start(void *, unsigned long);

int sys_execv(char *, char **, int *);

void * sys_sbrk(intptr_t, int *);

struct segment_entry * find_heap_segment(void);

#endif /* _SYSCALL_H_ */
