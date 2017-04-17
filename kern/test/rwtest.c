/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/test161.h>
#include <spinlock.h>

/*
 * Use these stubs to test your reader-writer locks.
 */

static struct rwlock *test_rwl = NULL;

static bool test_status = TEST161_FAIL;

int rwtest(int nargs, char **args) {
	(void)nargs;
	(void)args;

	test_rwl = rwlock_create("testlock");
	if (test_rwl == NULL) {
		panic("rwlock is null");
	}
	//
	rwlock_acquire_read(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_release_read(test_rwl);

	// kprintf_n("Should panic if successful...\n");
	rwlock_destroy(test_rwl);
	test_rwl = NULL;

	test_status = TEST161_SUCCESS;
	success(test_status, SECRET, "rwt1");

	return 0;
}

int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;

	test_rwl = rwlock_create("testlock");
	if (test_rwl == NULL) {
		panic("rwlock is null");
	}

	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);

	rwlock_destroy(test_rwl);
	test_rwl = NULL;

	success(TEST161_SUCCESS, SECRET, "rwt2");

	return 0;
}

int rwtest3(int nargs, char **args) {
	(void)nargs;
	(void)args;

	test_rwl = rwlock_create("testlock");

	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);

	rwlock_destroy(test_rwl);
	test_rwl = NULL;

	success(TEST161_SUCCESS, SECRET, "rwt3");

	return 0;
}

int rwtest4(int nargs, char **args) {
	(void)nargs;
	(void)args;

	test_rwl = rwlock_create("testlock");

	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);

	rwlock_destroy(test_rwl);
	test_rwl = NULL;

	success(TEST161_SUCCESS, SECRET, "rwt4");

	return 0;
}

int rwtest5(int nargs, char **args) {
	(void)nargs;
	(void)args;

	test_rwl = rwlock_create("testlock");

	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_write(test_rwl);
	rwlock_release_write(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);
	rwlock_acquire_read(test_rwl);
	rwlock_release_read(test_rwl);

	rwlock_destroy(test_rwl);
	test_rwl = NULL;

	success(TEST161_SUCCESS, SECRET, "rwt5");

	return 0;
}
