/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VM_H_
#define _VM_H_
#include <addrspace.h>
#include <machine/vm.h>

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

// Structure for coremap entry
struct coremap_page {

    // Current state of this block
    enum stateEnum {FREE, KERNEL, USER} state;

    struct page_entry * owner;

    // Series of blocks following this page:
    unsigned long block_size;
};

// Starting address for the coremap
paddr_t coremap_startaddr;

// Starting address for the coremap physically pages
paddr_t coremap_pagestartaddr;

// Swap stuff
struct vnode * swap_vnode;
struct bitmap * disk_bitmap;
bool can_swap;
unsigned long swap_disk_pages;

int block_read(int);
int block_write(int, );
int swap_in(struct page_entry *);
int swap_out(struct page_entry *);


// The number of pages in the coremap
unsigned int COREMAP_PAGES;
unsigned int current_page;

unsigned int pages_alloc;
unsigned int segments_alloc;
unsigned int addrs_alloc;

// Array based coremap
struct coremap_page *coremap;

// If the vm manager has booted
bool vm_booted;

/* Initialization function for coremap */
void coremap_bootstrap(void);
paddr_t calculate_range(unsigned int);

/* Initialization function */
void vm_bootstrap(void);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

struct segment_entry * find_segment_from_vaddr(vaddr_t);
struct page_entry * find_page_on_segment(struct segment_entry *, vaddr_t);

/* Helper function to get 'n' number of physical pages */
paddr_t getppages(unsigned long, bool);

void set_page_owner(struct page_entry *, paddr_t);

/* Free page */
void freeppage(paddr_t);

// Helper function to get physical address from virtual address.
// paddr_t get_paddr_from_vaddr(vaddr_t vaddr);

// Helper function to remove data from a page.
void as_zero_region(paddr_t paddr, unsigned npages);

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t vaddr);

/*
 * Return amount of memory (in bytes) used by allocated coremap pages.  If
 * there are ongoing allocations, this value could change after it is returned
 * to the caller. But it should have been correct at some point in time.
 */
unsigned int coremap_used_bytes(void);

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown(const struct tlbshootdown *);


#endif /* _VM_H_ */
