#include <types.h>
#include <vm.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>

/*
 * Wrap ram_stealmem in a spinlock.
 */
// static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;

/* Helper for calculating number of pages. This does all the math computation */
paddr_t calculate_range(unsigned int pages) {
	// |-----------------l---l---------------------------------------------|
	// | / / Coremap / / l---l ---l---l---l---l Pages l---l---l---l---l--- |
	// |-----------------l---l---------------------------------------------|
	//
	// range = (n * SOCP + (PAGE_SIZE - (n * SOCP % PAGE_SIZE))) + (PAGE_SIZE * n)
	// where
	// n = number of pages
	// SOCP = size of a single coremap_page struct (used for array length)
	// PAGE_SIZE = 4K size of a single page
	// range = size range of memory
        unsigned int pg = pages;
	paddr_t padding = PAGE_SIZE - ((pg * sizeof(struct coremap_page)) % PAGE_SIZE);
        unsigned long page_array_size = PAGE_SIZE * pages; 
	unsigned long coremap_size = pages * sizeof(struct coremap_page);
	unsigned long coremap_padded = coremap_size + padding;
        (void) coremap_padded;
        (void) page_array_size;
	paddr_t size = page_array_size + coremap_padded;
        return size;
}

/*
 * This function will bootstrap the coremap and pages, by first determining
 * the total number of pages needed. This is done by dividing the range of
 * addresses had (denoted by lastpaddr) by PAGE_SIZE. The real number of pages
 * will be
 */
void coremap_bootstrap() {

	// Physical memory address
  // Get the first and last free address to manage
	paddr_t last_address = ram_getsize();
	paddr_t first_address = ram_getfirstfree();

	// The range for the coremap
	paddr_t addr_range = last_address - first_address;

	//         pages          coremap
	//  mem     coremap     -------------- pad coremap to 4K --    physical pages
	// range = (n * SOCP + (PAGE_SIZE - (n * SOCP % PAGE_SIZE))) + (PAGE_SIZE * n)
	unsigned int pages = 1;

	// Keep trying to find the upper bound on pages
	while (addr_range > calculate_range(pages)) {
		pages++;
	}

	// We hit over with the 'while' loop, so we reduce
	// by one to bring back into acceptable memory range
	pages--;
	KASSERT(pages != 0);

	kprintf("Number of Pages Allocated: %d\n", pages);

	// How many pages we have (for iteration and shit).
	COREMAP_PAGES = pages;

	// Set the coremap to start at the starting address
	coremap_startaddr = first_address;
	coremap = (void*) PADDR_TO_KVADDR(coremap_startaddr);

	// We will initialize the current page
	// to be the first page
	current_page = 0;

	// The starting address for the physical pages
	paddr_t padding = PAGE_SIZE - ((pages * sizeof(struct coremap_page)) % PAGE_SIZE);
	coremap_pagestartaddr = first_address + (pages * sizeof(struct coremap_page) + padding);

	// Initialize the coremap with everything being unitialized
	for (unsigned int i=0; i<COREMAP_PAGES; i++) {
		coremap[i].allocated = false;
	}

	vm_booted = false;

}


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

paddr_t getppages(unsigned long npages) {
	// Cycle through the pages, try to get space raw
	// Could not get enough mem, time to swap!

	unsigned long count = 0;

	for (unsigned int i=0; i<COREMAP_PAGES; i++) {

		// Find an unallocated page
		if (!coremap[i].allocated) {
			count++;
		} else {
			count = 0;
		}

		// If we have what we need
		if (count == npages) {
			// Get the starting address
			int page_num = i - (count - 1);

			// Mark those pages as allocated
			for (unsigned int j=page_num; j < count; j++) {
				coremap[page_num].allocated = true;
			}

			// Return the correct address
			return (page_num * PAGE_SIZE) + coremap_pagestartaddr;
		}

	}

	return 0;
}

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages) {

	// Get address for n physical pages
	paddr_t addr = getppages(npages);

	// Make sure its valid
	if (addr != 0) {
		return PADDR_TO_KVADDR(addr);
	}
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
