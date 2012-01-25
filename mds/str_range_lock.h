/*
 * Copyright 2011-2012 the Redfish authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef REDFISH_STR_RANGE_LOCK_DOT_H
#define REDFISH_STR_RANGE_LOCK_DOT_H

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
