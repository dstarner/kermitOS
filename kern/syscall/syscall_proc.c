#include <types.h>
#include <addrspace.h>
#include <syscall.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <proc.h>
#include <current.h>
#include <thread.h>
#include <mips/trapframe.h>
#include <vnode.h>
#include <vfs.h>
#include <copyinout.h>
#include <kern/wait.h>


pid_t sys_getpid() {
	return curproc->pid;
}

pid_t sys_waitpid(pid_t pid, int *status, int options, int *err) {

	if (pid < 0 || pid > 256) {
		*err = ESRCH;
		return -1;
	}

	// Make sure process specified by pid exists
	if (procs[pid] == NULL) {
		return 0;
	}

	if(status == (int*) 0x0) {
		return 0;
	}

	if(status == (int*) 0x40000000 || status == (int*) 0x80000000 || ((int)status & 3) != 0) {
		*err = EFAULT;
		return -1;
	}

	if (options != 0 && options != WNOHANG && options != WUNTRACED) {
		*err = EINVAL;
		return -1;
	}

	// Make sure current proc is a parent of PID process
	if (curproc->pid != procs[pid]->parent_pid) {
		*err = ECHILD;
		return -1;
	}

	// Option handling goes here
	if (options == WNOHANG) {
		return 0;
	}

	// If invalid PID
	if (pid < 0 || pid > 127) {
		*err = ESRCH;
		return -1;
	}

	// If PID process is NULL
	if (procs[pid] == NULL) {
		*err = ESRCH;
		return -1;
	}


	lock_acquire(procs[pid]->e_lock);

	// If the process with pid can exit already, return the status.
	if (!procs[pid]->can_exit) {

		cv_wait(procs[pid]->e_cv, procs[pid]->e_lock);
		// cv_wait(curproc->e_cv, curproc->e_lock);
	}

	// Update status if status exists
	*status = procs[pid]->exit_code;

	// Release the lock
	lock_release(procs[pid]->e_lock);

	// We Good to clean up and destroy the parent
	// Destroy Addrspace
	as_destroy(procs[pid]->p_addrspace);
	procs[pid]->p_addrspace = NULL;

	// Destroy synch stuff
	cv_destroy(procs[pid]->e_cv);
	lock_destroy(procs[pid]->e_lock);

	// Destroy proc
	kfree(procs[pid]->p_name);
	kfree(procs[pid]);
	procs[pid] = NULL;

	return pid;
}

void new_thread_start(void *tf, unsigned long addr) {

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
	curproc->p_addrspace = new_addr;
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
	if(new_tf == NULL){
		*err = ENOMEM;
		return -1;
	}

	// Copy trapframe from old process
	memcpy(new_tf, tf, sizeof(struct trapframe));

	// Copy address space from old process
	failure = as_copy(curproc->p_addrspace, &new_addr);

	// Error checking
	if(new_addr == NULL){
		kfree(new_tf);
		*err = ENOMEM;
		return -1;
	}

	new_proc = proc_new_child("proc_child");
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

int sys_execv(char *program, char **args, int *err) {
  struct addrspace *addr;          // Address space for program
  struct vnode *v;               // Program file node
  vaddr_t startpoint, stackptr;  // New stack pointer and entry point addresses
  int failure;                   // Error Handler

  // Make sure there is program
  if (program == NULL) {
    *err = EFAULT;
    return -1;
  }

  /* Copy into kernelspace */
  char *name_copy = (char *) kmalloc(sizeof(char) * NAME_MAX);
  size_t actual = 0;

  // Copy the instructions
  failure = copyinstr((userptr_t)program, name_copy, NAME_MAX, &actual);

  if (failure) {
    *err = failure;
    return -1;
  }

  // Make sure there are arguments
  if (args == NULL) {
    *err = EFAULT;
    return -1;
  }

  if (strlen(name_copy) == 0) {
    *err = EINVAL;
    kfree(name_copy);
    return -1;
  }

  // Make sure valid pointers
  if ((int *)args == (int *)0x40000000 || (int *)args == (int *)0x80000000) {
    *err = EFAULT;
    kfree(name_copy);
    return -1;
  }

  // How many arguments the program has
  int num_of_args = 0;
  while (args[num_of_args] != NULL) {
    num_of_args++;

    // Make sure its within number of args
    if (num_of_args > ARG_MAX) {
      *err = E2BIG;
      kfree(name_copy);
      return -1;
    }
  }

  // Where the arguments will be copied for kernel space
  char **kernel_args = (char **) kmalloc(sizeof(char *) * num_of_args);

  int copied_args = 0;

  // Make sure that all the arguments are valid pointers
  while (copied_args < num_of_args) {
    if ((int *)args[copied_args] == (int *)0x40000000 || (int *)args[copied_args] == (int *)0x80000000) {
      *err = EFAULT;
      kfree(name_copy);
      return -1;
    }
    copied_args++;

  }

  // Padding is how much to change the stack pointer
  int arg_size = 0, padding = 0;

  for (copied_args=0; copied_args < num_of_args; copied_args++) {
    // Get size of argument
    arg_size = strlen(args[copied_args]) + 1;

    // Create and copy size
    kernel_args[copied_args] = (char *) kmalloc(sizeof(char) * arg_size);
    copyinstr((userptr_t)args[copied_args], kernel_args[copied_args], arg_size, &actual);

    // Make sure that the padding is correctly allocated
    padding += arg_size;
    if (padding % 4) { padding += (4 - (padding % 4)) % 4; }
  }

  // Open the program
  failure = vfs_open(name_copy, O_RDONLY, 0, &v);
  if (failure) {
    kfree(name_copy);
    *err = failure;
    return -1;
  }

  // Lets start switching over memory space
  addr = curproc->p_addrspace;
  curproc->p_addrspace = NULL;

  // Make sure memory deletion works
  as_destroy(addr);
  KASSERT(proc_getas() == NULL);

  addr = as_create();
  if (addr == NULL) {
    kfree(name_copy);
    vfs_close(v);
    *err = ENOMEM;
    return -1;
  }

  // Change and activate addressspace
  proc_setas(addr);
  as_activate();

  /* Load the executable. */
  failure = load_elf(v, &startpoint);
  if (failure) {
    kfree(name_copy);
    vfs_close(v);
    *err = failure;
    return -1;
  }

  failure = as_define_stack(addr, &stackptr);
  if (failure) {
    kfree(name_copy);
    *err = failure;
    return -1;
    }

  // Increment stack pointer for how much space is needed
  stackptr -= padding;
  char **arg_address = (char **) kmalloc(sizeof(char *) * num_of_args + 1);

  vfs_close(v);

  for(int i = 0; i < num_of_args; i++) {

    // Copy args to user stack
    arg_size = strlen(kernel_args[i]) + 1;

    // Adjust for stack padding
    if (arg_size % 4) {
      arg_size += (4 - arg_size % 4) % 4;
    }

    // Where to put the arg
    arg_address[i] = (char *)stackptr;

    // Copy to userspace
    copyoutstr(kernel_args[i], (userptr_t)stackptr, arg_size, &actual);
    stackptr += arg_size;
  }

  // According to the man pages, args end with '0'
  arg_address[num_of_args] = 0;

  // Readjust stack pointer to the start
  stackptr -= padding;
  stackptr -= (num_of_args + 1) * sizeof(char *);

  // Finish copying addresses to userspace
  for (int i = 0; i < num_of_args + 1; i++) {
    copyout((arg_address + i), (userptr_t)stackptr, sizeof(char *));
    stackptr += sizeof(char *);
  }

  // Again, reset the stack pointer to the start
  stackptr -= ((num_of_args + 1) * sizeof(char *));

  // Some cleanup needed
  kfree(kernel_args);
  kfree(name_copy);
  kfree(arg_address);

  // TO USER MODE WE GOOOOOOOOO!!!!!!!
  enter_new_process(num_of_args, (userptr_t) stackptr,
                   (userptr_t) stackptr, stackptr, startpoint);

  // We should never hit this, but what the hell, we like error handling
  *err = EINVAL;
  return -1;

}

void sys_exit(int exit_code, bool fatal_signal) {

  // Get the lock
  lock_acquire(curproc->e_lock);

  // Let parent know that it is trying to exit
  curproc->can_exit = true;

  // Handle fatal signal or clean exit
  if (fatal_signal) {
    curproc->exit_code = _MKWAIT_SIG(exit_code);
  } else {
    curproc->exit_code = _MKWAIT_EXIT(exit_code);
  }

  if (curproc->parent_pid < 0) {
    lock_release(curproc->e_lock);
  
   // DESTROY IT ALL! (I'm tired and just want this shit to work)
   cv_destroy(curproc->e_cv);
   lock_destroy(curproc->e_lock);

   // Destroy Address space
   as_destroy(curproc->p_addrspace);
   curproc->p_addrspace = NULL;

   // Destory proc
   kfree(procs[curproc->pid]->p_name);
   kfree(procs[curproc->pid]);
   procs[curproc->pid] = NULL;

  }

  if (!procs[curproc->parent_pid]->can_exit) {
    // Let all the waiting processes know that we are waiting.
    cv_broadcast(curproc->e_cv, curproc->e_lock);

  // } else {
  //   kprintf("exit call exit cleanup\n");
  //
  //   // Close out all of the current files
  //   for (int fd = 0; fd < OPEN_MAX; fd++) {
  //     int close_error = 0;  // We need to pass a fake val
  //     sys_close(fd, &close_error);
  //     kprintf("exit call close file\n");
  //   }
  //
  //   // kprintf("exit call close files\n");
  //   // Release the lock
  }

  lock_release(curproc->e_lock);
  thread_exit();

}
