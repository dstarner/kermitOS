#include <types.h>
#include <addrspace.h>
#include <syscall.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <proc.h>
#include <current.h>
#include <thread.h>


pid_t sys_getpid() {
  return curproc->pid;
}

void new_thread_start(void *tf, unsigned long addr){

  struct trapframe user_frame;
  struct trapframe* new_tf = (struct trapframe*) tf;
  struct addrspace* new_addr = (struct addrspace*) addr;

  // Set up the new trapframe
  new_tf->tf_v0 = 0;
  new_tf->tf_a3 = 0;
  new_tf->tf_epc += 4;

  // Copy from kern to user frame
  memcpy(&user_frame, new_tf, sizeof(struct trapframe));
  kfree(new_tf);
  new_tf = NULL;
  curproc->p_addrspace = childaddr;
  as_activate();

  // To user mode we gooooo!!!!!
  mips_usermode(&user_frame);

}

int sys_fork(struct trapframe* tf, int *err) {

  // Things we will need later
  int failure = 0;
  struct proc* new_proc;
  struct addrspace* new_addr = NULL;
  struct trapframe* new_tf = NULL;

  // Create new trapframe
  new_tf = kmalloc(sizeof(struct trapframe));
  if(childtf == NULL){
    *err = ENOMEM;
    return -1;
  }

  memcpy(new_tf, tf, sizeof(struct trapframe));
  failure = as_copy(curproc->p_addrspace, &new_addr);
  if(new_addr == NULL){
    kfree(new_tf);
    *err = ENOMEM;
    return -1;
  }

  new_proc = proc_create_child("proc_child");
  if(new_proc == NULL){
    kfree(new_tf);
    *err = ENOMEM;
    return -1;
  }

  failure = thread_fork("process", new_proc, new_thread_start, new_tf, (unsigned long) new_addr);
  if (failure) {
    return failure;
  }

  failure = new_proc->pid;
  new_proc->p_cwd = curproc->p_cwd;
  VOP_INCREF(curproc->p_cwd);
  curproc->p_numthreads++;
  return failure;

}

void sys_exit(int exitcode) {

  (void) exitcode;

  // Destroy addressspace
  as_destroy(curproc->p_addrspace);
  // Destroy process
  kfree(procs[curproc->pid]->p_name);
  curproc->p_addrspace = NULL;

  kfree(procs[curproc->pid]);

  procs[curproc->pid] = NULL;
  lock_destroy(curproc->e_lock);

  thread_exit();
}
