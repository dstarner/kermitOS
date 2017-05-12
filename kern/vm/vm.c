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
#include <linkedlist.h>
#include <vfs.h>
#include <vnode.h>
#include <stat.h>
#include <bitmap.h>
#include <device.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <uio.h>
#include <kern/iovec.h>

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

  // Initialize LRU pointer
  lru_pointer = 0;

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
    coremap[i].owner = NULL;
  }

  vm_booted = false;

}


/* Initialization function */
void vm_bootstrap() {

  // Swap disk name
  char * swap_disk_name = (char *) "lhd0raw:";

  int vnode_fail = vfs_open(swap_disk_name, O_RDWR, 0664, &(swap_vnode));

  // If we can't read the disk, then return
  if (vnode_fail) {
    can_swap = false;
    vm_booted = true;
    return;
  }

  // Stat for checking size
  struct stat stats;

  int stat_failure = VOP_STAT(swap_vnode, &stats);;
  // Make connection to swap disk
  //   a. If can't read size, disable swapping

  if (stat_failure) {
    can_swap = false;
    vm_booted = true;
    return;
  }

  //   b Else enable swapping
  can_swap = true;

  // If swapping, create bitmap size of disk / 4K (use vop_stat for size)
  off_t swap_disk_size = stats.st_size;
  swap_disk_pages = swap_disk_size / PAGE_SIZE;

  if (swap_disk_pages < 1) {
    can_swap = false;
    vm_booted = true;
    return;
  }

  // Create the bitmap
  disk_bitmap = bitmap_create(swap_disk_pages);
  // Set up bitmap
  KASSERT(disk_bitmap != NULL);

  // Initialize above here
  vm_booted = true;

  bitmap_lock = lock_create("bitmap lock");

  // Make sure we really booted
  KASSERT(vm_booted); // wot
}

void invalidate_tlb() {
  /* Disable interrupts on this CPU while frobbing the TLB. */
  int spl = splhigh();

       /* Invalidate everything in the TLB */
  for (unsigned int i=0; i<NUM_TLB; i++) {
    tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
  }

  splx(spl);
}

void add_entry_to_tlb(vaddr_t faultaddress, paddr_t paddr) {
  // kprintf("+");

  // I believe this part attempts to find an available TLB page entry and caches
  // it to the TLB.
  /* Disable interrupts on this CPU while frobbing the TLB. */
  int spl = splhigh();
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
    return;
  }

  // If the TLB is full, pick a random to evict
  ehi = faultaddress;
  elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
  tlb_random(ehi, elo);
  splx(spl);
}

int block_read(unsigned int swap_disk_index, paddr_t write_to_paddr) {

  // Create UIO and IOVec
  struct uio reader_uio;
  struct iovec reader_iovec;
  int remaining = PAGE_SIZE; // The remaining bytes to read (these are pages)
  // unsigned long coremap_index = (write_to_paddr - coremap_pagestartaddr) / PAGE_SIZE;

  // Determine where to store the data being read.
  reader_iovec.iov_ubase = (void *) PADDR_TO_KVADDR(write_to_paddr);
  reader_iovec.iov_len = PAGE_SIZE;

  reader_uio.uio_iov = &reader_iovec;
  reader_uio.uio_iovcnt = 1;

  // Set up for reading
  reader_uio.uio_rw = UIO_READ;
  reader_uio.uio_segflg = UIO_SYSSPACE;
  reader_uio.uio_resid = PAGE_SIZE;

  // There is no address space for this read operation.
  reader_uio.uio_space = NULL;

  // Find the offset of the page stored in the swapdisk
  reader_uio.uio_offset = swap_disk_index * PAGE_SIZE;

  // Read operations
  int result = VOP_READ(swap_vnode, &reader_uio);
  // Update amount of data transferred.
  remaining -= reader_uio.uio_resid;

  //KASSERT(result == 0 && remaining == 0);
  (void) result;
  return 0;
}


int block_write(unsigned int swap_disk_index, paddr_t read_from_paddr) {

  // Create UIO and IOVec
  struct uio writer_uio;
  struct iovec writer_iovec;
  int remaining = PAGE_SIZE; // The remaining bytes to read (these are pages)
  // unsigned long coremap_index = (read_from_paddr - coremap_pagestartaddr) / PAGE_SIZE;

  // Determine where to store the data being read.
  writer_iovec.iov_ubase = (void *) PADDR_TO_KVADDR(read_from_paddr);
  writer_iovec.iov_len = PAGE_SIZE;

  writer_uio.uio_iov = &writer_iovec;
  writer_uio.uio_iovcnt = 1;

  // Set up for writing
  writer_uio.uio_rw = UIO_WRITE;
  writer_uio.uio_segflg = UIO_SYSSPACE;
  writer_uio.uio_resid = PAGE_SIZE;

  // There is no address space for this read operation.
  writer_uio.uio_space = NULL;

  // Find the offset of the page stored in the swapdisk
  writer_uio.uio_offset = swap_disk_index * PAGE_SIZE;

  // kprintf("Reading From %x\n", read_from_paddr);
  // kprintf("Coremap index: %lu\n", coremap_index);
  // kprintf("Writing offset: %x\n\n", swap_disk_index * PAGE_SIZE);

  // Write operations
  int result = VOP_WRITE(swap_vnode, &writer_uio);
  // Update amount of data transferred.
  remaining -= writer_uio.uio_resid;

  //KASSERT(result == 0 && remaining == 0);
  (void) result;
  return 0;
}

int swap_in(struct page_entry * page) {
  KASSERT(page->swap_state == DISK);

  if (vm_booted) spinlock_acquire(&coremap_lock);

  unsigned long page_to_evict = (unsigned long) select_page_to_evict();

  //kprintf("%zu page found\n", select_page_to_evict());
  struct page_entry * old_page = coremap[page_to_evict].owner;

  // If the page selected to be evicted is not freed, that means we need to swap
  // it out.
  if (coremap[page_to_evict].state != FREE) {
    if (vm_booted) spinlock_release(&coremap_lock);
    swap_out(old_page);
    if (vm_booted) spinlock_acquire(&coremap_lock);
  }

  // Now that the page is swapped out, we can set up to swap in the page.
  unsigned int swap_in_bitmap_index = page->bitmap_disk_index;

  // Make sure that it is actually in disk
  KASSERT(bitmap_isset(disk_bitmap, swap_in_bitmap_index));

  // Try to swap in
  // kprintf("IN %x\n", page->vpage_n);
  if (vm_booted) spinlock_release(&coremap_lock);
  paddr_t paddr = (page_to_evict * PAGE_SIZE) + coremap_pagestartaddr;
  int error = block_read(swap_in_bitmap_index, paddr);
  if (vm_booted) spinlock_acquire(&coremap_lock);
  // kprintf("IN2 %x\n", page->vpage_n);

  lock_acquire(bitmap_lock);
  bitmap_unmark(disk_bitmap, swap_in_bitmap_index);
  lock_release(bitmap_lock);

  coremap[page_to_evict].state = USER;
  coremap[page_to_evict].owner = page;
  page->swap_state = MEMORY;
  page->ppage_n = paddr;
  page->state = CLEAN;

  KASSERT(error == 0);
  KASSERT(page->swap_state == MEMORY);

  if (vm_booted) spinlock_release(&coremap_lock);

  return 0;
}

int swap_out(struct page_entry * page) {
  KASSERT(page->swap_state == MEMORY);
  // kprintf("Swapping out...\n");

  if (vm_booted) spinlock_acquire(&coremap_lock);
  // lock_acquire(page->swap_lock);
  // Get a bitmap index
  unsigned int bitmap_index;

  // Prevent any other processes trying to write to the same block.
  lock_acquire(bitmap_lock);
  bitmap_alloc(disk_bitmap, &bitmap_index);
  lock_release(bitmap_lock);

  //kprintf("Swap out to %u\n", bitmap_index);
  //kprintf("Swapping out page at %x, %x\n", page->vpage_n, page->ppage_n);

  // Update the page entry
  page->swap_state = DISK;
  page->bitmap_disk_index = bitmap_index;

  // Try to swap out the page
  if (vm_booted) spinlock_release(&coremap_lock);
  int error = block_write(bitmap_index, page->ppage_n);
  if (vm_booted) spinlock_acquire(&coremap_lock);
  // freeppage(page->ppage_n);

  // Zero the page
  as_zero_region(page->ppage_n, 1);

  KASSERT(error == 0);
  KASSERT(page->swap_state == DISK);

  // lock_acquire(page->swap_lock);
  if (vm_booted) spinlock_release(&coremap_lock);
  invalidate_tlb();

  return 0;
}

uint32_t select_page_to_evict() {
  // Attempt to find a free page first.
  for (unsigned long i=0; i<COREMAP_PAGES; i++) {
    if (coremap[i].state == FREE) {
      return i;
    }
  }

  // return select_page_to_evict_random();
  return select_page_to_evict_clock_lru();
}

uint32_t select_page_to_evict_clock_lru() {
  // If there are no more available memory locations, find a page that is not
  // recently used.

  unsigned int initial_lru_pointer = lru_pointer++;
  uint32_t selected_page = lru_pointer % COREMAP_PAGES;

  while (coremap[selected_page].state != USER) {
    if (coremap[selected_page].owner != NULL && coremap[selected_page].owner->lru_used) {
      continue;
    }

    // If we made a whole rotation and there is no page to choose, use the random
    // algorithm.
    if (initial_lru_pointer == lru_pointer) {
      // Unset all the page's pointers here.
      for (unsigned int i = 0; i < COREMAP_PAGES; i++) {
        if (coremap[i].owner != NULL) {
          coremap[i].owner->lru_used = false;
        }
      }

      return select_page_to_evict_random();
    }

    // Pick a new page if the page is actively used.
    if (++lru_pointer > COREMAP_PAGES) {
      lru_pointer = 0;
    }

    selected_page = lru_pointer % COREMAP_PAGES;
  }

  // Unset all the page's pointers here.
  for (unsigned int i = 0; i < COREMAP_PAGES; i++) {
    if (coremap[i].owner != NULL) {
      coremap[i].owner->lru_used = false;
    }

  }
  return selected_page;
}


uint32_t select_page_to_evict_random() {
  // If there are no more available memory locations, swap out a random page.

  // kprintf("No free pages in coremap.\n");
  // If not enough pages are found, swapout!
  uint32_t random_page = random() % COREMAP_PAGES;

  while (coremap[random_page].state != USER) {
    random_page = random() % COREMAP_PAGES;
    // kprintf("@");
  }

  KASSERT(coremap[random_page].state != KERNEL);
  return random_page;
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

  vaddr_t old_addr = faultaddress;

  // First align the fault address to the starting address of the page
  faultaddress &= PAGE_FRAME;

  // Try to find the physical address translation first.
  // If the page isn't found, then it will become 0.
  struct segment_entry * seg = find_segment_from_vaddr(old_addr);

  // If a segment is not found, that means the vaddr given is out of the bounds
  // of the allocated regions and should return an error.
  // TODO: Stack overflow vs heap out-of-bounds
  if (seg == NULL) {
    // kprintf("\n==============\n\n");
    // kprintf("\nFault at 0x%x\n\n", old_addr);
    //
    // as = proc_getas();
    //
    // for (unsigned int i=0; i < ll_num(as->segments_list); i++) {
    //   struct segment_entry * seg = ll_get(as->segments_list, i);
    //   if (seg->executable) {kprintf("CODE/TEXT: Executable, ");}
    //   if (seg->writeable) {kprintf("Writeable, ");}
    //   if (seg->readable) {kprintf("Readable, ");}
    //   kprintf("0x%x --> 0x%x\n", seg->region_start, seg->region_start + seg->region_size);
    // }

    return EFAULT;
  }

  KASSERT(seg != NULL);

  // If fault address is valid, check if fault address is in Page Table
  struct page_entry * page = find_page_on_segment(seg, faultaddress);

  // if (page == NULL) {
  //   kprintf("\nFault2 at 0x%x\n\n", old_addr);
  //   return EFAULT;
  // }

  switch (faulttype) {
    case VM_FAULT_READ:
        //kprintf("Reading page at 0x%x\n", faultaddress);
      // If the page isn't found, there's something wrong and there is a
      // segmentation fault.
      if (page == NULL) {
        //kprintf("Requested 0x%x, so adding page to cover 0x%x -> 0x%x.\n", old_addr, faultaddress, (faultaddress + PAGE_SIZE)-1);
        // Allocate a new physical page
        paddr = getppages(1, false);

        // If a page cannot be acquired, then just say there's no memory...
        if (paddr == 0) {
          return ENOMEM;
        }

        // Create a new page entry to reference the physical page that was
        // just requested.
        page = (struct page_entry *) kmalloc(sizeof(struct page_entry));
        KASSERT(page);

        // Set the values of the new page created.
        page->ppage_n = paddr;
        page->vpage_n = faultaddress;
        page->state = CLEAN; // If a page is writable then assume it's dirty.
        page->bitmap_disk_index = 0;
        page->swap_state = MEMORY;
        page->lru_used = false;
        page->swap_lock = lock_create("swap_lock");

        set_page_owner(page, paddr);
        ll_add(seg->page_table, page, NULL);
      }

      paddr = page->ppage_n;
      break;

    case VM_FAULT_WRITE:
      // If no page, then create a new PTE and allocate a new physical page
      // dynamically.
      if (page == NULL) {

        //kprintf("Requested 0x%x, so adding page to cover 0x%x -> 0x%x.\n", old_addr, faultaddress, (faultaddress + PAGE_SIZE)-1);
        // Allocate a new physical page
        paddr = getppages(1, false);

        // If a page cannot be acquired, then just say there's no memory...
        if (paddr == 0) {
          return ENOMEM;
        }

        // Create a new page entry to reference the physical page that was
        // just requested.
        page = (struct page_entry *) kmalloc(sizeof(struct page_entry));
        KASSERT(page);

        // Set the values of the new page created.
        page->ppage_n = paddr;
        page->vpage_n = faultaddress;
        page->state = DIRTY; // If a page is writable then assume it's dirty.
        page->bitmap_disk_index = 0;
        page->swap_state = MEMORY;
        page->lru_used = false;
        page->swap_lock = lock_create("swap_lock");

        set_page_owner(page, paddr);
        ll_add(seg->page_table, page, NULL);
      } else {

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

  // If the page is on disk
  if (page->swap_state == DISK) {
    KASSERT(can_swap);

    // SWAP!
    int error = swap_in(page);
    // kprintf("Swapped in page addr: %x, %x\n", page->vpage_n, page->ppage_n);
    page->swap_state = MEMORY;
    page->bitmap_disk_index = 0;

    paddr = page->ppage_n; // Get the new physical address.

    KASSERT(error == 0);
  } else {
    // If it reaches here, that means the page is already available.
    // Just grab the physical address translation from the page and put
    // it in the paddr variable to add it to the TLB later.
    paddr = page->ppage_n;
  }

  // Update the LRU in the coremap.
  page->lru_used = true;

  // At this point the paddr needs to exist or else it would not have gotten
  // this far.
  // kprintf("FAULT %x, %x\n", faultaddress, paddr);
  if (paddr == 0) {
    // kprintf("faulttype %d vaddr %x -> paddr %x\n", faulttype, faultaddress, paddr);
    kprintf("FAULT %x, %x\n", faultaddress, paddr);
    return ENOMEM;
  }

  add_entry_to_tlb(faultaddress, paddr);

  KASSERT(paddr != 0);
  return 0;
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
  struct linkedlist * segs = as->segments_list;
  unsigned int i;
  for (i = 0; i < ll_num(segs); i++) {
    struct segment_entry * seg = ll_get(segs, i);
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
  struct linkedlist * pages = seg->page_table;
  KASSERT(pages != NULL);

  unsigned int i;
  for (i = 0; i < ll_num(pages); i++) {
    struct page_entry * page = ll_get(pages, i);

    // WHY IS THIS CAUSING THE WHOLE PROGRAM TO CRASH
    KASSERT(page != NULL);
    // if (page->vpage_n == 0) continue;

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

  if (!can_swap) {
    if (vm_booted) {
      spinlock_release(&coremap_lock);
    }
    return 0;
  }

  if (npages > 1) {
    if (vm_booted) {
      spinlock_release(&coremap_lock);
    }

    return 0;
  }

  // // If not enough pages are found, swapout!
  // uint32_t random_page = select_page_to_evict();
  //
  // // If booted, then be atomic
  // if (vm_booted) spinlock_release(&coremap_lock);
  // int error = swap_out(coremap[random_page].owner);
  // if (vm_booted) spinlock_acquire(&coremap_lock);
  //
  // // Fix the page if the page is created as a kernel page.
  // if (isKernel) coremap[random_page].state = KERNEL;
  //
  // KASSERT(error == 0);
  // paddr_t paddr = (random_page * PAGE_SIZE) + coremap_pagestartaddr;
  // KASSERT(can_swap);
  if (vm_booted) spinlock_release(&coremap_lock);
  //
  // return paddr;
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

void freeppage(paddr_t paddr) {
  unsigned long page_num = (paddr - coremap_pagestartaddr) / PAGE_SIZE;

  // If booted, then be atomic
  if (vm_booted) {
    spinlock_acquire(&coremap_lock);
  }

  // Make sure that the page is actually allocated
  KASSERT(coremap[page_num].state != KERNEL);

  // Free it
  coremap[page_num].state = FREE;
  coremap[page_num].block_size = 0;
  // set_page_owner(NULL, paddr);

  // If booted, then be atomic
  if (vm_booted) {
    spinlock_release(&coremap_lock);
  }

}


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
  // TODO: Not remove everything.
  invalidate_tlb();
  (void) shootdown;
}

void set_page_owner(struct page_entry * page, paddr_t address) {
  unsigned long page_num = (address - coremap_pagestartaddr) / PAGE_SIZE;

  // Make sure that the page is actually allocated
  // KASSERT(coremap[page_num].state != FREE);

  coremap[page_num].owner = page;
}
