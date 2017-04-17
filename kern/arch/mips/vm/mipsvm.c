#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

/*
 * coremap_bootstrap is located in ram.c so that it can easily get
 * memory space
 */

 /*
  * Wrap ram_stealmem in a spinlock.
  */
 static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;

/* Initialization function */
void vm_bootstrap() {

}

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress) {

  (void) faulttype;
  (void) faultaddress;
  return 0;
}

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages) {
  (void) npages;
  return 0;
}

void free_kpages(vaddr_t addr) {
  (void) addr;
}

/*
 * Return amount of memory (in bytes) used by allocated coremap pages.  If
 * there are ongoing allocations, this value could change after it is returned
 * to the caller. But it should have been correct at some point in time.
 */
unsigned int coremap_used_bytes() {
  return 0;
}

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown(const struct tlbshootdown * shootdown) {
  (void) shootdown;
}
