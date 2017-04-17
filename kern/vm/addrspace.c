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

#include <types.h>
#include <spl.h>
#include <kern/errno.h>
#include <lib.h>
#include <linkedlist.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <array.h>
#include <current.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

   // Create the segments list
	 as->segments_list = array_create();
	 if (as->segments_list == NULL) {
		 kfree(as);
		 return NULL;
	 }

	return as;
}


int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	(void) old;



	// 1. Copy over the page table and segments table


	*ret = newas;
	return 0;
}


void
as_activate(void)
{
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	 /* Disable interrupts on this CPU while frobbing the TLB. */
 	int spl = splhigh();

        /* Invalidate everything in the TLB */
 	for (unsigned int i=0; i<NUM_TLB; i++) {

		// If the TLB entry is valid, then mark the virtual page as DIRTY.


 		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
 	}

 	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{

	// Check if there will be overlap
	if (find_segment_from_vaddr(vaddr) != NULL) {
		return EINVAL;
	}

  // Create the actual segment itself
	struct segment_entry * segment = kmalloc(sizeof(struct segment_entry));

  // Set the start and bounds for the segment
  segment->region_start = vaddr;
	segment->region_size = memsize;

  // Set the permissions on this segment
	if (readable < 1) segment->readable = true;
	if (writeable < 1) segment->writable = true;
	if (executable < 1) segment->executable = true;

  // Initialize the page table
	segment->page_table = array_create();
	if (segment->page_table == NULL) {
		kfree(as);
		return ENOMEM;
	}

  // Add it to the array
	int result = array_add(as->segments_list, (void *) segment, NULL);
	if (result) {
		segment_destroy(segment);
		as_destroy(as);
		return result;
	}

	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

  // Just make all my asserts
	KASSERT(as != NULL && as == curproc->p_addrspace);
	return 0;
}

int
as_complete_load(struct addrspace *as)
{

	KASSERT(as != NULL);

	(void)as;
	return 0;
}

int as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{

  // Set up the stack
	int result = as_define_region(as, USERSTACKBASE, USERSTACKSIZE, 1, 1, 0);

	// If something happens, lets return it
 	if (result) {
 		return result;
 	}

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}


/* Destroy an address space and all its segments */
void as_destroy(struct addrspace *as)
{

  struct segment_entry * segment;

  // Iterate through each of the segments
	for (unsigned int i = 0; i < array_num(as->segments_list); i++) {

		// Get the segment and then destroy it.
		segment = (struct segment_entry *) array_get(as->segments_list, i);
    segment_destroy(segment);

	}

  // Destroy the array
	array_setsize(as->segments_list, 0);
	array_destroy(as->segments_list);

  // Delete the addres sspace
	kfree(as);
}


/* Destroy a segment and its page table */
void segment_destroy(struct segment_entry * segment) {

	// Iterate over the page table and destroy each one
	for (unsigned int i = 0; i < array_num(segment->page_table); i++) {

		// Get each page and free it
		struct page_entry * page = (struct page_entry *) array_get(segment->page_table, i);
		kfree(page);
	}

	// Destroy the array
	array_setsize(segment->page_table, 0);
	array_destroy(segment->page_table);

  // Free the segment
	kfree(segment);

}
