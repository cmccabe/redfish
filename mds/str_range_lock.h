/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_STR_RANGE_LOCK_DOT_H
#define ONEFISH_STR_RANGE_LOCK_DOT_H

#include <semaphore.h>

/* Range locking allows a set of threads to lock and unlock ranges, and wait
 * for other threads if the ranges are busy. There is an assumption here that
 * the maximum number of threads in the system is fixed, and the data
 * structures used reflect that.
 *
 * Locking is done on the half-open range [start, end).
 * So if you lock /foo/a to /foo/b, I can still lock /foo/b to /foo/c.
 */

#define STR_RANGE_LOCK_MAX_LOCKS 40

/** Represents a range lock */
struct str_range_lock {
	const char *start;
	const char *end;
	sem_t *sem;
};

/** Initialize the lock_range subsystem.
 *
 * Must be called before any calls are made to lock_range or unlock_range.
 *
 * Returns 0 on success; error code otherwise.
 */
int init_lock_range_subsystem(void);

/** Lock a range.
 *
 * This function will block on the provided semaphore until it can take a
 * lock on the provided range. When it returns, you will have the lock,
 * which you must dispose of when you're done with unlock_range.
 *
 * Returns 0 on success; error code otherwise.
 */
int lock_range(struct str_range_lock *lock);

/** Unlock a range.
 */
void unlock_range(struct str_range_lock *lock);

#endif
