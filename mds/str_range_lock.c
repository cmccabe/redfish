/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "mds/str_range_lock.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>

static pthread_spinlock_t g_spinlock;

static struct str_range_lock *g_locks[STR_RANGE_LOCK_MAX_LOCKS];
static int g_num_locks;

static struct str_range_lock *g_waiters[STR_RANGE_LOCK_MAX_LOCKS];
static int g_num_waiters;

int init_lock_range_subsystem(void)
{
	int ret;
	ret = pthread_spin_init(&g_spinlock, 0);
	if (ret)
		return ret;
	memset(g_locks, 0, sizeof(g_locks));
	g_num_locks = 0;
	memset(g_waiters, 0, sizeof(g_waiters));
	g_num_waiters = 0;
	return 0;
}

int lock_range(struct str_range_lock *me)
{
	int i, ret = 0;
	pthread_spin_lock(&g_spinlock);
	if (g_num_locks == STR_RANGE_LOCK_MAX_LOCKS) {
		ret = -ENOLCK;
		goto done;
	}
	for (i = 0; i < g_num_locks; ++i) {
		struct str_range_lock *lock = g_locks[i];
		if (strcmp(me->end, lock->start) < 0)
			continue;
		if (strcmp(me->start, lock->end) > 0)
			continue;
		break;
	}
	if (i != g_num_locks) {
		/* Wait for someone else to give us the lock */
		if (g_num_waiters == STR_RANGE_LOCK_MAX_LOCKS) {
			ret = -ENOLCK;
			goto done;
		}
		g_waiters[g_num_waiters++] = me;
		pthread_spin_unlock(&g_spinlock);
		sem_wait(me->sem);
		return 0;
	}
	else {
		/* Get the lock immediately */
		g_locks[g_num_locks++] = me;
	}
done:
	pthread_spin_unlock(&g_spinlock);
	return ret;
}

void unlock_range(struct str_range_lock *me)
{
	int i, j, ret = 0;
	pthread_spin_lock(&g_spinlock);
	for (i = 0; i < g_num_locks; ++i) {
		struct str_range_lock *lock = g_locks[i];
		if (lock == me)
			break;
	}
	if (i == g_num_locks) {
		ret = -EINVAL;
		goto done;
	}
	/* Can we wake anyone up? */
	for (j = 0; j < g_num_waiters; ++j) {
		struct str_range_lock *waiter = g_locks[j];
		if (strcmp(me->end, waiter->start) < 0)
			continue;
		if (strcmp(me->start, waiter->end) > 0)
			continue;
		break;
	}
	if (j != g_num_waiters) {
		/* Wake someone up */
		g_locks[i] = g_waiters[j];
		if (g_num_waiters != 1) {
			g_waiters[j] = g_waiters[g_num_waiters - 1];
		}
		g_num_waiters--;
		pthread_spin_unlock(&g_spinlock);
		sem_post(g_locks[i]->sem);
		return;
	}
	else {
		/* There are fewer locks now. */
		g_num_locks--;
	}
done:
	pthread_spin_unlock(&g_spinlock);
}

