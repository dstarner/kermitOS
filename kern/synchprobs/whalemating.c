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
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

// So, here I'm going to put all my thoughts....
// You need conditional variables to wait and signal when correct
// numbers are hit. You also need to keep track of the 
// number of each type


// GAMEPLAN:
// 1. when X is called, increment X_num.
// 2. If Y and Z have numbers greater than 0, then great! 
//    they can all mate.


// Lock to protect critial section of shared memory.
struct lock* cock_block_lock;

// The number of each type
unsigned int male_num; unsigned int female_num; unsigned int matchmaker_num;

// The conditional variables for each
struct cv* male_cv; struct cv* female_cv; struct cv* matchmaker_cv;


/*
 * Called by the driver during initialization.
 */

void whalemating_init() {

        // Create all of the cv's
        male_cv = cv_create("Male");
        female_cv = cv_create("Female");
        matchmaker_cv = cv_create("Matchmaker");

        // Create the lock
        cock_block_lock = lock_create("cock blocker");

        KASSERT(cock_block_lock != NULL);

        // Make sure nothing blew up
        KASSERT(male_cv != NULL && female_cv != NULL && matchmaker_cv != NULL);

}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {
        // CLEAN IT ALL UP!!!
        // ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
        // ░░░░░░░▄▄▀▀▀▀▀▀▀▀▀▀▄▄█▄░░░░▄░░░░█░░░░░░░
        // ░░░░░░█▀░░░░░░░░░░░░░▀▀█▄░░░▀░░░░░░░░░▄░
        // ░░░░▄▀░░░░░░░░░░░░░░░░░▀██░░░▄▀▀▀▄▄░░▀░░
        // ░░▄█▀▄█▀▀▀▀▄░░░░░░▄▀▀█▄░▀█▄░░█▄░░░▀█░░░░
        // ░▄█░▄▀░░▄▄▄░█░░░▄▀▄█▄░▀█░░█▄░░▀█░░░░█░░░
        // ▄█░░█░░░▀▀▀░█░░▄█░▀▀▀░░█░░░█▄░░█░░░░█░░░
        // ██░░░▀▄░░░▄█▀░░░▀▄▄▄▄▄█▀░░░▀█░░█▄░░░█░░░
        // ██░░░░░▀▀▀░░░░░░░░░░░░░░░░░░█░▄█░░░░█░░░
        // ██░░░░░░░░░░░░░░░░░░░░░█░░░░██▀░░░░█▄░░░
        // ██░░░░░░░░░░░░░░░░░░░░░█░░░░█░░░░░░░▀▀█▄
        // ██░░░░░░░░░░░░░░░░░░░░█░░░░░█░░░░░░░▄▄██
        // ░██░░░░░░░░░░░░░░░░░░▄▀░░░░░█░░░░░░░▀▀█▄
        // ░▀█░░░░░░█░░░░░░░░░▄█▀░░░░░░█░░░░░░░▄▄██
        // ░▄██▄░░░░░▀▀▀▄▄▄▄▀▀░░░░░░░░░█░░░░░░░▀▀█▄
        // ░░▀▀▀▀░░░░░░░░░░░░░░░░░░░░░░█▄▄▄▄▄▄▄▄▄██
        // ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
        cv_destroy(male_cv);
        cv_destroy(female_cv);
        cv_destroy(matchmaker_cv);
        lock_destroy(cock_block_lock);
}

void
male(uint32_t index)
{

        male_start(index);

        // Get the lock
        lock_acquire(cock_block_lock);
        // All of the checking code below here

        male_num++;

        if (matchmaker_num > 0 && female_num > 0) {
                // Reduce the male count
                male_num--;

                // Reduce female count
                female_num--;
                cv_signal(female_cv, cock_block_lock);
                
                // Reduce female count
                matchmaker_num--;
                cv_signal(matchmaker_cv, cock_block_lock);
        } else {
                // Wait until there is a suitable partner.
                // Good man, Mr. Whale, it is better to wait for Mrs Right!
                cv_wait(male_cv, cock_block_lock);

        }

        // ... and above here
        male_end(index);

        // Release the lock
        lock_release(cock_block_lock);
        

}

void
female(uint32_t index)
{

        female_start(index);

        // Get the lock
        lock_acquire(cock_block_lock);
        // All of the checking code below here

        female_num++;

        if (matchmaker_num > 0 && male_num > 0) {
                // Reduce the female count
                female_num--;

                // Reduce male count
                male_num--;
                cv_signal(male_cv, cock_block_lock);

                // Reduce matchmaker count
                matchmaker_num--;
                cv_signal(matchmaker_cv, cock_block_lock);
        } else {
                // Wait until there is a suitable partner.
                // Good man, Mrs. Whale, it is better to wait for Mr Right!
                cv_wait(female_cv, cock_block_lock);

        }

        // ... and above here

        female_end(index);
        // Release the lock
        lock_release(cock_block_lock);


}

void
matchmaker(uint32_t index)
{

        matchmaker_start(index);

        // Get the lock
        lock_acquire(cock_block_lock);
        // All of the checking code below here

        matchmaker_num++;

        if (male_num > 0 && female_num > 0) {
                // Reduce the male count
                matchmaker_num--;

                // Reduce female count
                female_num--;
                cv_signal(female_cv, cock_block_lock);

                // Reduce male count
                male_num--;
                cv_signal(male_cv, cock_block_lock);
        } else {
                // Wait until there is a suitable partner.
                // Good man, Mr. Whale, it is better to wait for Mrs Right!
                cv_wait(matchmaker_cv, cock_block_lock);

        }

        // ... and above here

        matchmaker_end(index);
        // Release the lock
        lock_release(cock_block_lock);


}
