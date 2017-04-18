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
#include <array.h>

/*
 * Wrap ram_stealmem in a spinlock.
 */
static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;

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
    coremap[i].state = FREE;
    coremap[i].block_size = 0;
  }

  vm_booted = false;

}


/* Initialization function */
void vm_bootstrap() {

  // Initialize above here
  vm_booted = true;

  // Make sure we really booted
  KASSERT(vm_booted); // wot
}

/* Fault handling function called by trap code */
/*
 * When vm_fault is called, that means the process attempted to find a
 * matching
 */
int vm_fault(int faulttype, vaddr_t faultaddress) {
  // Declare these variables for use later.
  paddr_t paddr = 0;
  struct addrspace *as;
  int spl;

  if (curproc == NULL) {
    /*
     * No process. This is probably a kernel fault early in boot. Return EFAULT
     * so as to panic instead of getting into an infinite faulting loop.
     */
    return EFAULT;
  }

  as = proc_getas();
  if (as == NULL) {
    /*
     * No address space set up. This is probably also a kernel fault early in boot.
     */
    return EFAULT;
  }

  // First align the fault address to the starting address of the page
  faultaddress &= PAGE_FRAME;

  // Try to find the physical address translation first.
  // If the page isn't found, then it will become 0.
  struct segment_entry * seg = find_segment_from_vaddr(faultaddress);

  // If a segment is not found, that means the vaddr given is out of the bounds
  // of the allocated regions and should return an error.
  // TODO: Stack overflow vs heap out-of-bounds
  if (seg == NULL) return EFAULT;

  // kprintf("VM Fault with perms E: %d, R: %d, W: %d at vaddr %x\n", seg->executable, seg->readable, seg->writeable, faultaddress);

  // If fault address is valid, check if fault address is in Page Table
  struct page_entry * page = find_page_on_segment(seg, faultaddress);

  switch (faulttype) {
    case VM_FAULT_READ:
      // If the page isn't found, there's something wrong and there is a
      // segmentation fault.
      if (page == NULL) return EFAULT;
      paddr = page->ppage_n;
      break;

    case VM_FAULT_WRITE:
      // If no page, then create a new PTE and allocate a new physical page
      // dynamically.
      if (page == NULL) {
        // Allocate a new physical page
        paddr = getppages(1, false);

        // Create a new page entry to reference the physical page that was
        // just requested.
        page = (struct page_entry *) kmalloc(sizeof(struct page_entry));

        // Set the values of the new page created.
        page->ppage_n = paddr;
        page->vpage_n = faultaddress;
        page->state = DIRTY; // If a page is writable then assume it's dirty.

        array_add(seg->page_table, page, NULL);

      } else {
        // If it reaches here, that means the page is already available.
        // Just grab the physical address translation from the page and put
        // it in the paddr variable to add it to the TLB later.
        paddr = page->ppage_n;
      }


      break;

    // A write was attempted on a read only page, which is an error.
    // That means the physical address on the TLB is dirty.
    case VM_FAULT_READONLY:
      // I don't know what to do here, so I'll just do the default case...
    default:
      return EINVAL;
  } // End of case switch

  // At this point the paddr needs to exist or else it would not have gotten
  // this far.

  KASSERT(paddr != 0);

  // I believe this part attempts to find an available TLB page entry and caches
  // it to the TLB.
  /* Disable interrupts on this CPU while frobbing the TLB. */
  spl = splhigh();
  uint32_t ehi, elo;
  int i;

  for (i=0; i<NUM_TLB; i++) {
    tlb_read(&ehi, &elo, i);
    if (elo & TLBLO_VALID) {
      continue;
    }
    ehi = faultaddress;
    elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
    tlb_write(ehi, elo, i);
    splx(spl);
    return 0;
  }

  // If the TLB is full, pick a random to evict
  ehi = faultaddress;
  elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
  tlb_random(ehi, elo);

  splx(spl);
  return EFAULT;
}

/*
 * Find a page entry on the provided segment. If it doesn't exist, it returns
 * NULL.
 */
struct segment_entry * find_segment_from_vaddr(vaddr_t vaddr) {
  // Make sure the process exists as we need to grab information from it.
  struct addrspace * as = proc_getas();
  KASSERT(as != NULL);

  // Iterate the array to find if there is a match.
  struct array * segs = as->segments_list;
  unsigned int i;
  for (i = 0; i < array_num(segs); i++) {
    struct segment_entry * seg = array_get(segs, i);
    if (seg->region_start <= vaddr &&
        seg->region_start + seg->region_size > vaddr) {
      return seg;
    }
  }

  return NULL;
}

/*
 * Find a page entry on the provided segment that contains the vaddr.
 * If it doesn't exist, it returns NULL.
 */
struct page_entry * find_page_on_segment(struct segment_entry * seg, vaddr_t vaddr) {
  // Make sure the process exists as we need to grab information from it.
  KASSERT(curproc != NULL);
  KASSERT(seg != NULL);

  // Iterate the array to find if there is a match.
  struct array * pages = seg->page_table;
  unsigned int i;
  for (i = 0; i < array_num(pages); i++) {
    struct page_entry * page = array_get(pages, i);
    if (((vaddr_t) (page->vpage_n)) == vaddr) {
      return page;
    }
  }

  return NULL;
}

void
as_zero_region(paddr_t paddr, unsigned npages)
{
  bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}


paddr_t getppages(unsigned long npages, bool isKernel) {
  // Cycle through the pages, try to get space raw
  // Could not get enough mem, time to swap!

  unsigned long count = 0;

  // If booted, then be atomic
  if (vm_booted) {
    spinlock_acquire(&coremap_lock);
  }

  for (unsigned long i=0; i<COREMAP_PAGES; i++) {

    // Find a series of unallocated page that matches npages.
    if (coremap[i].state == FREE) {
      count++;  // Increment the count
    } else {
      count = 0;  // Reset the count
    }

    // If we have what we need
    if (count == npages) {
      // Get the starting address
      unsigned long page_num = i - (count - 1);

      // Calculate the physical address for the first page allocated.
      paddr_t paddr = (page_num * PAGE_SIZE) + coremap_pagestartaddr;
      KASSERT(paddr != 0);

      // Mark those pages as allocated
      for (unsigned long j = 0; j < count; j++) {
        // Set if the page is user or kernel
        coremap[page_num + j].state = isKernel ? KERNEL : USER;

        // Initialize value for block_size for all pages.
        coremap[page_num + j].block_size = 0;

        // Clear out the page
        as_zero_region(((page_num + j) * PAGE_SIZE) + coremap_pagestartaddr, 1);
      }

      // Remember to set the block_size for the first page
      coremap[page_num].block_size = npages;

      // If booted, then be atomic
      if (vm_booted) {
        spinlock_release(&coremap_lock);
      }

      // Return the correct address
      return paddr;
    }
  }

  // If booted, then be atomic
  if (vm_booted) {
    spinlock_release(&coremap_lock);
  }

  // If not enough pages are found, return 0
  return 0;
}


/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages) {

  // Get address for n physical pages
  paddr_t addr = getppages(npages, true);

  // Make sure its valid
  if (addr != 0) {
    // Return the virtual kernel address
    return PADDR_TO_KVADDR(addr);
  }
  return 0;
}

// paddr_t get_paddr_from_vaddr(vaddr_t vaddr) {
//   uint32_t vaddr_vpn = vaddr >> 20; // Get virtual page from virtual address by getting top 20 bits.
//   uint32_t vaddr_offset = vaddr & 0xFFF; // Get offset of the virtual address; 0xFFF is 12 bits (1111 1111 1111)
//
//   // Find physical page on TLB
//   uint32_t ehi, elo; // According to tlb.h ENTRYLO is not used but still needs to be set.
//   ehi = vaddr_vpn;
//
//   // Find a matching TLB entry.
//   int tlb_index = tlb_probe(ehi, elo);
//
//   // tlb_probe returns -1 if it cannot find an index and the page table need to
//   // be
//   if (tlb_index < 0) {
//     // We need to find the physical address if the TLB cannot find a match.
//
//     for (unsigned long i = 0; i < COREMAP_PAGES; ++i) {
//       // If we found the page with the same virtual page number, then that means
//       // The index is the correct physical page on the coremap, and we need to
//       // convert the virtual address to the physical one.
//       if (coremap[i].virtual_page_num == vaddr_vpn) {
//         // Find the starting point of the physical page address.
//         uint32_t ppage_num = coremap_pagestartaddr + (i * PAGE_SIZE);
//
//         // Build the physical address by concatenting the two chunks together.
//         paddr_t paddr = (ppage_num << 20) + vaddr_offset;
//
//         // TODO: Write to TLB for caching.
//         return paddr;
//       }
//     }
//
//     // If the program has not returned yet, then the virtual address provided does not
//     // have an associated page.
//     kprintf("Kernel cannot find physical address");
//     return 0;
//   }
//
//   // At this point we can assume we found a match, and we need to read the TLB for the address.
//   // I'm going to assume tlb_probe is more efficient in finding matches than manually doing it.
//   tlb_read(&ehi, &elo, tlb_index); // elo will now contain the physical addr
//
//   // Build the physical address by concatenting the two chunks together.
//   paddr_t paddr = (elo << 20) + vaddr_offset;
//   return paddr;
// }

// void free_kpages(vaddr_t vaddr) {
  // paddr_t paddr = get_paddr_from_vaddr(vaddr);
  //
  // // If this returns 0 then that means it is not a valid virtual address.
  // // Virtual addresses shouldn't be valid at 0 anyways.
  // if (paddr == 0) {
  //   // TODO: Error catching?
  //   return;
  // }
  //
  // // At this point we found a physical address, and we can proceed to free the pages.
  // // Identify the physical page on the coremap array
  // unsigned ppage_coremap_index = (paddr - coremap_pagestartaddr) % PAGE_SIZE;
  //
  // // Determine that the page is no longer allocated.
  // coremap[ppage_coremap_index].allocated = false;

void free_kpages(vaddr_t addr) {
  paddr_t raw_paddr = KVADDR_TO_PADDR(addr);

  // 1) addr = (page_num * PAGE_SIZE) + coremap_pagestartaddr
  // 2) addr - coremap_pagestartaddr = page_num * PAGE_SIZE
  unsigned long page_num = (raw_paddr - coremap_pagestartaddr) / PAGE_SIZE;

  // If booted, then be atomic
  if (vm_booted) {
    spinlock_acquire(&coremap_lock);
  }

  // Make sure that the page is actually allocated
  KASSERT(coremap[page_num].state != FREE);


  unsigned long blocks = coremap[page_num].block_size;

  for (unsigned long offset = 0; offset < blocks; offset++) {
    coremap[page_num + offset].state = FREE;
    coremap[page_num + offset].block_size = 0;
  }


  // If booted, then be atomic
  if (vm_booted) {
    spinlock_release(&coremap_lock);
  }

}

/*
 * Return amount of memory (in bytes) used by allocated coremap pages.  If
 * there are ongoing allocations, this value could change after it is returned
 * to the caller. But it should have been correct at some point in time.
 */
unsigned int coremap_used_bytes() {
  unsigned count = 0;
  for (unsigned i = 0; i < COREMAP_PAGES; ++i) {
    if (coremap[i].state != FREE) ++count;
  }

  return count * PAGE_SIZE;
}

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown(const struct tlbshootdown * shootdown) {
  (void) shootdown;
}
