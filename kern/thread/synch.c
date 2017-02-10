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

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

// curthread gives the current thread

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
	if (sem == NULL) {
		return NULL;
	}

	sem->sem_name = kstrdup(name);
	if (sem->sem_name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

void
P(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	// Create the lock, return if null
	lock = kmalloc(sizeof(*lock));
	if (lock == NULL) {
		return NULL;
	}

	// Give name to lock, free if no name
	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}

	// Create the wait channel
	lock->lk_wchan = wchan_create(lock->lk_name);
	if (lock->lk_wchan == NULL) {  // Delete if something goes wrong
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}

	// Initialize the spinlock for the lock.
	spinlock_init(&lock->lk_spinlock);
       
        // Initialize the current thread 
        // holding the lock to null. 
        lock->lk_thread = NULL;


	return lock;
}

void
lock_destroy(struct lock *lock)
{
	KASSERT(lock != NULL);

        // If there is a thread currently holding the lock, 
        // then we fucked up
        KASSERT(lock->lk_thread == NULL);

	// add stuff here as needed
	// If there is a thread currently using the lock, PANIC!
	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&lock->lk_spinlock);
	wchan_destroy(lock->lk_wchan);

	kfree(lock->lk_name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{

	KASSERT(lock != NULL);

        // Aquire spinlock
        spinlock_acquire(&lock->lk_spinlock);

	// If not in control of the spinlock, sleep
	while (lock->lk_thread != NULL) {
		// Have the spinlock sleep
		wchan_sleep(lock->lk_wchan, &lock->lk_spinlock);

	}

	// Set the current thread.
	lock->lk_thread = curthread;

	// Release the spinlock
	spinlock_release(&lock->lk_spinlock);

}

void
lock_release(struct lock *lock)
{
	KASSERT(lock != NULL);
       
        // If the thread releasing the lock is not the 
        // current thread, then we fucked up. 
        KASSERT(lock->lk_thread == curthread);

	// Aquire spinlock
	spinlock_acquire(&lock->lk_spinlock);
	

        // Release current thread
	lock->lk_thread = NULL;
        
	// Wake up a spinlock
	wchan_wakeone(lock->lk_wchan, &lock->lk_spinlock);

	// Release the spinlock
	spinlock_release(&lock->lk_spinlock);

}

bool
lock_do_i_hold(struct lock *lock)
{
	KASSERT(lock != NULL);

	// Return if the lock is held or not.
	return lock->lk_thread == curthread;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstrdup(name);
	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}

	// Create the wait channel
	cv->cv_wchan = wchan_create(cv->cv_name);
	if (cv->cv_wchan == NULL) {  // Delete if something goes wrong
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;

        }
	
        // Initialize the spinlock for the cv.
	spinlock_init(&cv->cv_spinlock);

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);

        // Check to make sure its in a good state
	spinlock_cleanup(&cv->cv_spinlock);
	wchan_destroy(cv->cv_wchan);

	kfree(cv->cv_name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{

        // Make sure the lock is real and yours.
        KASSERT(lock != NULL);
        KASSERT(lock_do_i_hold(lock));        

        // Make sure the cv exists
        KASSERT(cv != NULL);

        // Lock down the process
        spinlock_acquire(&cv->cv_spinlock);

        // Release the lock
        lock_release(lock);        
        // Put thread to sleep on cv's wait channel till signalled
        wchan_sleep(cv->cv_wchan, &cv->cv_spinlock);
        
        // Release spinlock
        spinlock_release(&cv->cv_spinlock);
        
        // Reacquire lock
        lock_acquire(lock);

}

void
cv_signal(struct cv *cv, struct lock *lock)
{
        // Make sure the lock is real and yours.
        KASSERT(lock != NULL);
        KASSERT(lock_do_i_hold(lock));        

        // Make sure the cv exists
        KASSERT(cv != NULL);
        
        // Lock down the process
        spinlock_acquire(&cv->cv_spinlock);

        // Wake one of the threads on the waitchannel
	wchan_wakeone(cv->cv_wchan, &cv->cv_spinlock);
         
        // Release spinlock
        spinlock_release(&cv->cv_spinlock);

}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
        // Make sure the lock is real and yours.
        KASSERT(lock != NULL);
        KASSERT(lock_do_i_hold(lock));        

        // Make sure the cv exists
        KASSERT(cv != NULL);

        // Lock down the process
        spinlock_acquire(&cv->cv_spinlock);
        
        // Wake all of the threads on the wait channel
        wchan_wakeall(cv->cv_wchan, &cv->cv_spinlock);
        // Release spinlock
        spinlock_release(&cv->cv_spinlock);

}


///////////////////////////////////////////////////////
// 
//  RW Locks


struct rwlock * 
rwlock_create(const char *name) 
{
	struct rwlock *rw;

	rw = kmalloc(sizeof(*rw));
	if (rw == NULL) {
		return NULL;
	}

	rw->rw_name = kstrdup(name);
	if (rw->rw_name==NULL) {
		kfree(rw);
		return NULL;
	}

        // Create the cv
        rw->rw_cv = cv_create(rw->rw_name); 

        // Create the lock
        rw->rw_lock = lock_create(rw->rw_name);

        rw->b_read = 0;

        rw->w_wait = 0;

        return rw;
}

void 
rwlock_destroy(struct rwlock *lock) 
{

        KASSERT(lock != NULL);

        // Destroy essentials
        cv_destroy(lock->rw_cv);
        lock_destroy(lock->rw_lock);
	
        kfree(lock->rw_name);
	kfree(lock);

}

void 
rwlock_acquire_read(struct rwlock *lock)
{

        lock_acquire(lock->rw_lock);

        // While something is currently writing
        while(lock->w_wait == 1) {
                // ... wait
                cv_wait(lock->rw_cv, lock->rw_lock);
        }

        lock->b_read++;

        lock_release(lock->rw_lock);

}

void 
rwlock_release_read(struct rwlock *lock)
{
        
        lock_acquire(lock->rw_lock);

        // Decrement number of readers
        lock->b_read--;

        if (lock->b_read == 0) {
        
                // Signal on the CV
                cv_signal(lock->rw_cv, lock->rw_lock);

        }

        lock_release(lock->rw_lock);

}

void
rwlock_acquire_write(struct rwlock *lock)
{
        
        lock_acquire(lock->rw_lock);
        
        // While something is currently writing or reading
        while(lock->w_wait == 1 || lock->b_read > 0) {
                // ... wait
                cv_wait(lock->rw_cv, lock->rw_lock);
        }

        lock->w_wait = 1;

        lock_release(lock->rw_lock);

}

void 
rwlock_release_write(struct rwlock *lock)
{

        lock_acquire(lock->rw_lock);

        lock->w_wait = 0;

        cv_broadcast(lock->rw_cv, lock->rw_lock);

        lock_release(lock->rw_lock);

}
