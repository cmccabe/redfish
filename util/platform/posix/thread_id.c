/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/platform/thread_id.h"

#include <pthread.h>

static pthread_mutex_t unique_thread_id_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t next_unique_thread_id = 1;

uint32_t create_unique_thread_id(void)
{
	uint32_t ret;

	/* pthread_self is useless here because it doesn't define what kind of
	 * value it returns.
	 * So just create a unique thread ID on the spot.
	 */
	pthread_mutex_lock(&unique_thread_id_lock);
	ret = next_unique_thread_id;
	next_unique_thread_id++;
	pthread_mutex_unlock(&unique_thread_id_lock);
	return ret;
}
