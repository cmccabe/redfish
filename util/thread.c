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

#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_mgr.h"
#include "util/fast_log_types.h"
#include "util/platform/thread_id.h"
#include "util/thread.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

void* redfish_thread_trampoline(void *v)
{
	int ret;
	char name[FAST_LOG_BUF_NAME_MAX];
	struct redfish_thread *rt = (struct redfish_thread*)v;

	rt->thread_id = create_unique_thread_id();
	snprintf(name, FAST_LOG_BUF_NAME_MAX, "thread %d", rt->thread_id);
	fast_log_set_name(rt->fb, name);
	ret = rt->fn(rt);
	if (ret)
		return ERR_PTR(FORCE_POSITIVE(ret));
	else
		return 0;
}

int redfish_thread_create(struct fast_log_mgr *mgr, struct redfish_thread* rt,
		redfish_thread_fn_t fn, void *data, void *data2)
{
	int ret;

	memset(rt, 0, sizeof(struct redfish_thread));
	rt->fb = fast_log_create(mgr, "redfish_thread_buf");
	if (IS_ERR(rt->fb))
		return PTR_ERR(rt->fb);
	rt->fn = fn;
	rt->init_data = data;
	rt->init_data2 = data2;
	ret = pthread_create(&rt->pthread, NULL,
			redfish_thread_trampoline, rt);
	if (ret)
		return ret;
	return 0;
}

int redfish_thread_join(struct redfish_thread *rt)
{
	int ret;
	void *rval = NULL;

	if (!rt->fn) {
		/* thread was never started */
		return 0;
	}
	ret = pthread_join(rt->pthread, &rval);
	if (ret)
		return -ret;
	fast_log_free(rt->fb);
	if (IS_ERR(rval))
		return PTR_ERR(rval);
	else
		return 0;
}
