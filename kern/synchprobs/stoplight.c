/*
 * Copyright (c) 2001, 2002, 2009
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

/*
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
*
*    SOME HELPER FUNCTIONS DEFINED
*
*/


void make_right(struct lock *, uint32_t dir, uint32_t index);
void make_straight(struct lock *, struct lock *, uint32_t dir, uint32_t index);
void make_left(struct lock *, struct lock *, struct lock *, uint32_t dir, uint32_t index);


// The intersection as a whole
struct semaphore * intersection;

// Locks ensuring one car per quadrant
struct lock * quad0_lock;
struct lock * quad1_lock;
struct lock * quad2_lock;
struct lock * quad3_lock;

/*
 * Called by the driver during initialization.
 */

void
stoplight_init() {

        intersection = sem_create("Intersection", 3);

        quad0_lock = lock_create("Quadrant 0");
        quad1_lock = lock_create("Quadrant 1");
        quad2_lock = lock_create("Quadrant 2");
        quad3_lock = lock_create("Quadrant 3");

        // Make sure there's an intersection
        KASSERT(intersection != NULL);
        
        // Make sure we have some form of order
        KASSERT(quad0_lock != NULL && quad1_lock != NULL && quad2_lock != NULL && quad3_lock != NULL);
}

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {

        // Cleanup
        sem_destroy(intersection);

        // Destroy locks
        lock_destroy(quad0_lock);
        lock_destroy(quad1_lock);
        lock_destroy(quad2_lock);
        lock_destroy(quad3_lock);

}


void make_right(struct lock * start, uint32_t dir, uint32_t index) {

        lock_acquire(start);

        inQuadrant(dir, index);

        leaveIntersection(index);

        lock_release(start);

}


void make_straight(struct lock * start, struct lock * end, uint32_t dir, uint32_t index) {

       // Start moving
       lock_acquire(start);

       // Inform test of our location
       inQuadrant(dir, index);

       // Move into final quadrant
       lock_acquire(end);

       // Inform of updated quadrant
       inQuadrant((dir + 3) % 4, index);

       // Leave start quadrant
       lock_release(start);

       leaveIntersection(index);

       // Free up quadrant
       lock_release(end);

}


void make_left(struct lock * start, struct lock * mid, struct lock * end, uint32_t dir, uint32_t index) {

        // Move into intersection
        lock_acquire(start);

        // Tell test
        inQuadrant(dir, index);

        // Move into next quad
        lock_acquire(mid);
        inQuadrant((dir + 3) % 4, index);

        // Leave start quad
        lock_release(start);

        // Move into last quad
        lock_acquire(end);
        inQuadrant((dir + 2) % 4, index);

        // Leave mid quad
        lock_release(mid);

        leaveIntersection(index);
     
        // Leave the last quad
        lock_release(end);

}


void
turnright(uint32_t direction, uint32_t index)
{

        // Tell the semaphore a car (thread) is coming
        P(intersection);

        struct lock * lock_to_use = NULL;

        // Welp, crappy if/else ifs....
        if (direction == 0) {
                lock_to_use = quad0_lock;
        } else if (direction == 1) {
                lock_to_use = quad1_lock;
        } else if (direction == 2) {
                lock_to_use = quad2_lock;
        } else if (direction == 3) {
                lock_to_use = quad3_lock;
        }

        // Perform the action
        make_right(lock_to_use, direction, index); 

        // Car is safely leaving intersection
        V(intersection);

}
void
gostraight(uint32_t direction, uint32_t index)
{
        // Tell the semaphore a car (thread) is coming
        P(intersection);
        
        struct lock * start = NULL;
        struct lock * end = NULL;

        // Welp, crappy if/else ifs....
        if (direction == 0) {
                start = quad0_lock;
                end = quad3_lock;
        } else if (direction == 1) {
                start = quad1_lock;
                end = quad0_lock;
        } else if (direction == 2) {
                start = quad2_lock;
                end = quad1_lock;
        } else if (direction == 3) {
                start = quad3_lock;
                end = quad2_lock;
        }
        
        // Perform the action
        make_straight(start, end, direction, index); 

        // Car is safely leaving intersection
        V(intersection);
}
void
turnleft(uint32_t direction, uint32_t index)
{
        // Tell the semaphore a car (thread) is coming
        P(intersection);
       
        struct lock * start = NULL;
        struct lock * mid = NULL;
        struct lock * end = NULL;

        // Welp, crappy if/else ifs....
        if (direction == 0) {
                start = quad0_lock;
                mid = quad3_lock;
                end = quad2_lock;
        } else if (direction == 1) {
                start = quad1_lock;
                mid = quad0_lock;
                end = quad3_lock;
        } else if (direction == 2) {
                start = quad2_lock;
                mid = quad1_lock;
                end = quad0_lock;
        } else if (direction == 3) {
                start = quad3_lock;
                mid = quad2_lock;
                end = quad1_lock;
        }

        // Perform the action
        make_left(start, mid, end, direction, index); 
        
        V(intersection);
}
