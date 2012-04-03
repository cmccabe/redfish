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

#include "util/platform/thread_id.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>

#define INVALID_TID 0
#define MAX_TID 0xffffffff

static pthread_key_t g_tid_key;

static pthread_mutex_t g_thread_id_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t g_next_tid = INVALID_TID;

/*
 * This rather complex implementation of get_tid is necessitated by the fact
 * that:
 *
 * 1. pthread_self doesn't define what kind of value it returns.  It may be a
 * number, but it might even be a structure with multiple fields.  So we can't
 * use it as an id.  To be honest, this seems like a goofy decision by the
 * standards committee.  Couldn't the thread id just have been a 64-bit number?
 * The implementors who really wanted to use a structure could just have
 * returned a pointer as the ID-- it would have worked just fine.  But no, it's
 * an "opaque datatype"-- and the comparison function they provide doesn't even
 * provide an ordering, just equal/not equal.  Unusable.
 *
 * 2. There's no equivalent to PTHREAD_MUTEX_INITIALIZER for pthread_key_t.
 * pthread_key_t needs to be initialized before it can be used.  And since this
 * is the only portable way to get thread-local data, we have to deal with that
 * here.
 */

void unique_thread_id_init(void)
{
	int ret;

	pthread_mutex_lock(&g_thread_id_lock);
	if (g_next_tid != INVALID_TID) {
		/* The mutex includes a memory barrier that will make other
		 * threads' stores visible to us.  So if g_next_tid
		 * has already been initialized, but wasn't visible to us
		 * before, it will be now. */
		return;
	}
	if (pthread_key_create(&g_tid_key, NULL))
		abort();
	g_next_tid = 1;
	pthread_mutex_unlock(&g_thread_id_lock);
}

uint32_t get_tid(void)
{
	uint32_t tid;

	if (g_next_tid == INVALID_TID) {
		unique_thread_id_init();
	}
	tid = (uint32_t)(uintptr_t)pthread_getspecific(g_tid_key);
	if (tid == INVALID_TID) {
		pthread_mutex_lock(&g_thread_id_lock);
		tid = g_next_tid++;
		if (tid == MAX_TID)
			abort();
		pthread_mutex_unlock(&g_thread_id_lock);
		if (pthread_setspecific(g_tid_key, (void*)(uintptr_t)tid))
			abort();
	}
	return tid;
}
