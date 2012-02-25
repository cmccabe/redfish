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

#ifndef REDFISH_SRANGE_LOCK_DOT_H
#define REDFISH_SRANGE_LOCK_DOT_H

#include <semaphore.h> /* for sem_t */

/* Range locking allows a set of threads to lock and unlock ranges, and wait
 * for other threads if the ranges are busy. There is an assumption here that
 * the maximum number of threads in the system is fixed, and the data
 * structures used reflect that.
 *
 * Locking is done on the half-open range [start, end).
 * So if you lock /foo/a to /foo/b, I can still lock /foo/b to /foo/c.
 */

#define SRANGE_LOCKER_MAX_RANGE 2

struct srange_tracker;

struct srange {
	const char *start;
	const char *end;
};

/** Represents a range lock */
struct srange_locker {
	sem_t *sem;
	int num_range;
	struct srange range[SRANGE_LOCKER_MAX_RANGE];
};

/** Create a string range tracker.
 *
 * @param max_lockers	Maximum number of string range lockers to
 *			support on this tracker.
 *
 * @return		A string range tracker on success, or an error
 *			pointer.
 */
extern struct srange_tracker* srange_tracker_init(int max_lockers);

/** Free a string range tracker.
 *
 * @param tk		The string range tracker
 */
extern void srange_tracker_free(struct srange_tracker *tk);

/** Lock a set of string ranges.
 *
 * This function will block on the provided semaphore until it can take a
 * lock on the provided range. When it returns, you will have the lock,
 * which you must dispose of when you're done with unlock_range.
 *
 * @param tk		The string range tracker
 * @param lk		The string range locker
 *
 * @return		0 on success; ENOLCK if there are already too many
 *			waiters being tracked (in this case, please increase
 *			max_lockers)
 */
extern int srange_lock(struct srange_tracker *tk, struct srange_locker *lk);

/** Unlock a set of string ranges.
 *
 * @param tk		The string range tracker
 * @param lk		The string range locker
 */
extern void srange_unlock(struct srange_tracker *tk, struct srange_locker *lk);

#endif
