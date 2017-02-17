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

	// add stuff here as needed
	// Create wchan
	lock->lk_wchan = wchan_create(lock->lk_name);
	if (lock->lk_wchan == NULL) {
		// Free the memory if no wchan is created.
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}

	// Initiate spinlock.
	spinlock_init(&lock->lk_lock);

	// Instantiate thread to null, to allow threads to acquire the lock.
	lock->lk_thread = NULL;

	HANGMAN_LOCKABLEINIT(&lock->lk_hangman, lock->lk_name);

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	KASSERT(lock != NULL);

	// Need to make sure the lock does not have any active threads before it
	// is destroyed.
	KASSERT(lock->lk_thread == NULL);

	// add stuff here as needed
	// Clean up memory when the lock is destroyed.
	spinlock_cleanup(&lock->lk_lock);

	wchan_destroy(lock->lk_wchan);

	kfree(lock->lk_name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	// Write this
	KASSERT(lock != NULL); // Make sure lock exists.
	KASSERT(curthread->t_in_interrupt == false); // May not block in an interrupt handler.

	spinlock_acquire(&lock->lk_lock);

	/* Call this (atomically) before waiting for a lock */
	HANGMAN_WAIT(&curthread->t_hangman, &lock->lk_hangman);

	// Keep repeating while the lock has an active thread.
	while	(lock->lk_thread != NULL) {
		// Sleep all threads awaiting in the wait channel.
		wchan_sleep(lock->lk_wchan, &lock->lk_lock);
	}

	// Change the current thread in lock to match the current thread running.
	// The lock's current thread will be NULL at this point, but the wchan will
	// have woken up one of the threads.
	KASSERT(lock->lk_thread == NULL);
	lock->lk_thread = curthread;

	spinlock_release(&lock->lk_lock);

	/* Call this (atomically) once the lock is acquired */
	HANGMAN_ACQUIRE(&curthread->t_hangman, &lock->lk_hangman);
}

void
lock_release(struct lock *lock)
{
	/* Call this (atomically) when the lock is released */
	HANGMAN_RELEASE(&curthread->t_hangman, &lock->lk_hangman);

	// Write this
	KASSERT(lock != NULL);

	// Make sure only the thread who owns the lock can release it.
	KASSERT(lock->lk_thread == curthread);

	spinlock_acquire(&lock->lk_lock);

	// Remove the current thread from the lock so the lock can be acquired.
	lock->lk_thread = NULL;

	// Wake a thread up to become the next current thread of the lock.
	wchan_wakeone(lock->lk_wchan, &lock->lk_lock);

	spinlock_release(&lock->lk_lock);

  /* Call this (atomically) when the lock is released */
	HANGMAN_RELEASE(&curthread->t_hangman, &lock->lk_hangman);
}

bool
lock_do_i_hold(struct lock *lock)
{
	KASSERT(lock != NULL);

	// To check if the lock is held by a thread, check if the current global thread
	// is the same thread as the one in the lock.
	return curthread == lock->lk_thread;
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

	// add stuff here as needed
	// Create wchan
	cv->cv_wchan = wchan_create(cv->cv_name);
	if (cv->cv_wchan == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

	// Init spinlock for CV
	spinlock_init(&cv->cv_lock);

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);

  // Check to make sure its in a good state
	spinlock_cleanup(&cv->cv_lock);
	wchan_destroy(cv->cv_wchan);

	kfree(cv->cv_name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	// Write this
	cv_sanity_check(cv, lock); // Run sanity check.

	// Release the supplied lock, go to sleep, and then when you wake up re-acquire the lock.
	spinlock_acquire(&cv->cv_lock);

	lock_release(lock);
	wchan_sleep(cv->cv_wchan, &cv->cv_lock);

	spinlock_release(&cv->cv_lock);

	// Reacquire the lock
	lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	// Write this
	cv_sanity_check(cv, lock); // Run sanity check.

	// Make sure only 1 wchan is woken up at a time
	spinlock_acquire(&cv->cv_lock);

	wchan_wakeone(cv->cv_wchan, &cv->cv_lock); // Wake a thread on CV's wchan.

	spinlock_release(&cv->cv_lock);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
	cv_sanity_check(cv, lock); // Run sanity check.

	spinlock_acquire(&cv->cv_lock);

	wchan_wakeall(cv->cv_wchan, &cv->cv_lock); // Wake all threads on CV's wchan.

	spinlock_release(&cv->cv_lock);
}

void
cv_sanity_check(struct cv *cv, struct lock *lock)
{
	// Make sure components exist
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);

	// Make sure the lock is currently acquired by the CV
	KASSERT(lock_do_i_hold(lock));
}

struct rwlock *
rwlock_create(const char *name)
{
	struct rwlock *rwlock;
	rwlock = kmalloc(sizeof(*rwlock));
	if (rwlock == NULL) {
		return NULL;
	}

	rwlock->rwlock_name = kstrdup(name);
	if (rwlock->rwlock_name == NULL) {
		kfree(rwlock);
		return NULL;
	}

	// Initialize rwlock internal structure
	rwlock->rwlock_lock = lock_create(name);
	if (rwlock->rwlock_lock == NULL) {
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}

	rwlock->rwlock_cv_readers = cv_create(name);
	if (rwlock->rwlock_cv_readers == NULL) {
		kfree(rwlock->rwlock_lock);
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}

	rwlock->rwlock_cv_writers = cv_create(name);
	if (rwlock->rwlock_cv_writers == NULL) {
		kfree(rwlock->rwlock_lock);
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}

	rwlock->readers_count = 0;
	rwlock->read_mode = 1;
	rwlock->has_writer = 0;
	rwlock->writers_queued = 0;

	return rwlock;
}

void
rwlock_destroy(struct rwlock *rwlock)
{
	// Sanity checks
	KASSERT(rwlock != NULL);

	// Can only free memory if there is no readers and writers
	cv_destroy(rwlock->rwlock_cv_readers);
	cv_destroy(rwlock->rwlock_cv_writers);
	lock_destroy(rwlock->rwlock_lock);

	kfree(rwlock->rwlock_name);
	kfree(rwlock);
}

void
rwlock_acquire_read(struct rwlock *rwlock)
{
	// Sanity checks
	KASSERT(rwlock != NULL);

	lock_acquire(rwlock->rwlock_lock);

	// Switch rwlock mode if a set amount of time had passed in a mode.
	// Make sure there are no writers before reading the file.
	while (rwlock->has_writer || !rwlock->read_mode) {
		cv_wait(rwlock->rwlock_cv_readers, rwlock->rwlock_lock);
	}

	// Increase the amount of readers.
	rwlock->readers_count++;

	lock_release(rwlock->rwlock_lock);
}

void
rwlock_release_read(struct rwlock *rwlock)
{
	// Sanity checks
	KASSERT(rwlock != NULL);
	KASSERT(rwlock->readers_count > 0);

	lock_acquire(rwlock->rwlock_lock);

	rwlock->readers_count--;

	// Attempt to wake a thread. There may be other threads reading.
	cv_broadcast(rwlock->rwlock_cv_readers, rwlock->rwlock_lock);

	if (rwlock->writers_queued > 0) {
		rwlock->read_mode = 0;
		cv_signal(rwlock->rwlock_cv_writers, rwlock->rwlock_lock);
	}

	lock_release(rwlock->rwlock_lock);
}

void
rwlock_acquire_write(struct rwlock *rwlock)
{
	lock_acquire(rwlock->rwlock_lock);

	rwlock->writers_queued++;

	// Make sure there are no writers before reading the file.
	while (rwlock->has_writer || rwlock->readers_count > 0) {
		cv_wait(rwlock->rwlock_cv_writers, rwlock->rwlock_lock);
	}

	// Increase the amount of writers.
	rwlock->has_writer = 1;
	rwlock->writers_queued--;

	lock_release(rwlock->rwlock_lock);
}

void
rwlock_release_write(struct rwlock *rwlock)
{
	// Sanity checks
	KASSERT(rwlock != NULL);
	KASSERT(rwlock->has_writer != 0);

	lock_acquire(rwlock->rwlock_lock);

	// Attempt to wake all threads, in this situation there is nothing
	// trying to read or write.
	rwlock->has_writer = 0;
	rwlock->read_mode = 1;
	cv_broadcast(rwlock->rwlock_cv_readers, rwlock->rwlock_lock);
	cv_signal(rwlock->rwlock_cv_writers, rwlock->rwlock_lock);

	lock_release(rwlock->rwlock_lock);
}
