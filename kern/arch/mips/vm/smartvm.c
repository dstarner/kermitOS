#include <machine/vm.h>
#include <vm.h>

/* Initialization function */
void vm_bootstrap() {

}

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress) {

}

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages) {

}

void free_kpages(vaddr_t addr) {

}

/*
 * Return amount of memory (in bytes) used by allocated coremap pages.  If
 * there are ongoing allocations, this value could change after it is returned
 * to the caller. But it should have been correct at some point in time.
 */
unsigned int coremap_used_bytes() {

}

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown(const struct tlbshootdown *) {

}
