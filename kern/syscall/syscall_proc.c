#include <types.h>
#include <syscall.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <current.h>


pid_t getpid(void) {
  return curproc->pid;
}
