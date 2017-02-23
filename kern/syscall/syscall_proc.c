#include <types.h>
#include <syscall.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <proc.h>
#include <current.h>
#include <thread.h>


pid_t sys_getpid(void) {
  return curproc->pid;
}

void sys_exit(int exitcode) {

  // Destroy addressspace
  as_destroy(curproc->p_addrspace);
  // Destroy process
  kfree(procs[curproc->pid]->p_name);
  curproc->p_addrspace = NULL;

  kfree(proc_ids[curproc->pid]);

  procs[curproc->pid] = NULL;
  lock_destroy(curproc->e_lock);

  thread_exit();
}
