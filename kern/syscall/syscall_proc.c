#include <types.h>
#include <syscall.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <proc.h>
#include <current.h>


pid_t sys_getpid() {
  return curproc->pid;
}
