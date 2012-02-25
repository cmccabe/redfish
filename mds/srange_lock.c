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

#include "mds/srange_lock.h"
#include "util/error.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>

struct srange_tracker {
	/** Size of lockers array */
	int max_lockers;
	/** Current number of lockers */
	int num_lockers;
	/** Current number of waiters */
	int num_waiters;
	/** Lock which protects lockers */
	pthread_spinlock_t lock;
	/** Current lockers */
	struct srange_locker *lockers[0];
	/** Following: current waiters */
};

inline static struct srange_locker **get_waiters(struct srange_tracker *tk)
{
	return tk->lockers + tk->max_lockers;
}

static int srange_has_overlap(const struct srange_locker *la,
				const struct srange_locker *lb)
{
	int i, j;
	const struct srange *ra;
	const struct srange *rb;

	for (i = 0; i < la->num_range; ++i) {
		ra = &la->range[i];
		for (j = 0; j < lb->num_range; ++j) {
			rb = &lb->range[j];
			if (strcmp(ra->end, rb->start) < 0)
				continue;
			if (strcmp(ra->start, rb->end) > 0)
				continue;
			return 1;
		}
	}
	return 0;
}

struct srange_tracker *srange_tracker_init(int max_lockers)
{
	int ret;
	struct srange_tracker *tk;

	tk = calloc(1, sizeof(struct srange_tracker) +
			(sizeof(struct srange_locker) * max_lockers));
	if (!tk)
		return ERR_PTR(ENOMEM);
	tk->max_lockers = max_lockers;
	ret = pthread_spin_init(&tk->lock, 0);
	if (ret) {
		free(tk);
		return ERR_PTR(ret);
	}
	return tk;
}

void srange_tracker_free(struct srange_tracker *tk)
{
	free(tk);
}

int srange_lock(struct srange_tracker *tk, struct srange_locker *lk)
{
	int i, res, ret = 0;
	int num_lockers;
	struct srange_locker **waiters;

	pthread_spin_lock(&tk->lock);
	num_lockers = tk->num_lockers;
	if (num_lockers == tk->max_lockers) {
		ret = -ENOLCK;
		goto done;
	}
	for (i = 0; i < num_lockers; ++i) {
		if (!srange_has_overlap(lk, tk->lockers[i]))
			break;
	}
	if (i != num_lockers) {
		/* Wait for someone else to give us the lock */
		if (tk->num_waiters == tk->max_lockers) {
			ret = -ENOLCK;
			goto done;
		}
		waiters = get_waiters(tk);
		waiters[tk->num_waiters++] = lk;
		pthread_spin_unlock(&tk->lock);
		RETRY_ON_EINTR(res, sem_wait(lk->sem));
		return 0;
	}
	else {
		/* Get the lock immediately */
		tk->lockers[num_lockers] = lk;
		tk->num_lockers++;
	}
done:
	pthread_spin_unlock(&tk->lock);
	return ret;
}

void srange_unlock(struct srange_tracker *tk, struct srange_locker *lk)
{
	int i, j, POSSIBLY_UNUSED(ret);
	struct srange_locker **waiters;
		
	ret = 0;
	pthread_spin_lock(&tk->lock);
	for (i = 0; i < tk->num_lockers; ++i) {
		if (lk == tk->lockers[i])
			break;
	}
	if (i == tk->num_lockers) {
		ret = -EINVAL;
		goto done;
	}
	/* Can we wake anyone up? */
	waiters = get_waiters(tk);
	for (j = 0; j < tk->num_waiters; ++j) {
		if (srange_has_overlap(lk, waiters[j]))
			break;
		break;
	}
	if (j != tk->num_waiters) {
		/* Wake someone up */
		tk->lockers[i] = waiters[j];
		if (tk->num_waiters != 1) {
			waiters[j] = waiters[tk->num_waiters - 1];
		}
		tk->num_waiters--;
		pthread_spin_unlock(&tk->lock);
		sem_post(tk->lockers[i]->sem);
		return;
	}
	else {
		/* There are fewer locks now. */
		tk->num_lockers--;
	}
done:
	pthread_spin_unlock(&tk->lock);
}
