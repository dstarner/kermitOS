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
 * This function will bootstrap the coremap and pages, by first determining
 * the total number of pages needed. This is done by dividing the range of
 * addresses had (denoted by lastpaddr) by PAGE_SIZE. The real number of pages
 * will be
 */
void coremap_bootstrap() {

  // Get the first free address to manage
	paddr_t first_address = ram_getfirstfree();
	paddr_t last_address = ram_getsize();

	// The range for the coremap
	paddr_t addr_range = last_address - first_address;

	unsigned int total_pages = addr_range / PAGE_SIZE;

	// (latpaddr - firstfree) = n * size(coremap_block) + PAGE_SIZE * n
	// where n is the number of pages
	unsigned int pages = sizeof(struct coremap_page) * total_pages / PAGE_SIZE;

	// Make sure it fits in a nice 4K block
	if ((sizeof(struct coremap_page) * total_pages) % PAGE_SIZE > 0) {
		pages++;
	}

	// Grab all of the memory
	coremap_startaddr = ram_stealmem(pages);
  KASSERT(coremap_startaddr != 0)

  // Initialize the coremap at that location
  coremap = (void*) PADDR_TO_KVADDR(coremap_startaddr);

	// Allocat for the coremap
	int allocated_pages = firstpaddr / PAGE_SIZE;
	for(int i = 0; i < allocated_pages; i++) {
		coremap[i].allocated = true;
	}

	// Everything else is free game
	int free_pages = total_pages - allocated_pages;
	for(int i = allocated_pages; i < free_pages; i++) {
		coremap[i].allocated = false;
	}

	vm_booted = false;

}

 /*
  * Wrap ram_stealmem in a spinlock.
  */
 static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;

/* Initialization function */
void vm_bootstrap() {

	// Initialize above here
	vm_booted = true;

	// Make sure we really booted
	KASSERT(vm_booted);
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
