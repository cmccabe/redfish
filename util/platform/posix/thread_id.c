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

#include <pthread.h>

static pthread_mutex_t unique_thread_id_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t next_unique_thread_id = 1;

uint32_t create_unique_thread_id(void)
{
	uint32_t ret;

	/* pthread_self is useless here because it doesn't define what kind of
	 * value it returns.  It may be a number, but it might even be a
	 * structure with multiple fields.
	 *
	 * So we just create a unique thread ID on the spot here.
	 */
	pthread_mutex_lock(&unique_thread_id_lock);
	ret = next_unique_thread_id;
	next_unique_thread_id++;
	pthread_mutex_unlock(&unique_thread_id_lock);
	return ret;
}
