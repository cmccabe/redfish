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
#include "util/compiler.h"
#include "util/error.h"
#include "util/test.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRANGE_LOCK_UNIT_MAX_LOCKERS 40

static int simple_test(void)
{
	sem_t sem;
	struct srange_locker lk;
	struct srange_tracker *tk;

	tk = srange_tracker_init(SRANGE_LOCK_UNIT_MAX_LOCKERS);
	EXPECT_NOT_ERRPTR(tk);
	memset(&lk, 0, sizeof(lk));
	lk.sem = &sem;
	lk.num_range = 1;
	lk.range[0].start = "/a/";
	lk.range[0].end = "/a0";
	EXPECT_ZERO(sem_init(&sem, 0, 0));
	EXPECT_ZERO(srange_lock(tk, &lk));
	srange_unlock(tk, &lk);
	EXPECT_ZERO(sem_destroy(&sem));
	srange_tracker_free(tk);

	return 0;
}

static struct srange_tracker *g_test2_tracker;
static sem_t g_test2_parent_sem;
static volatile int g_test2_thread_got_range;

static void* test2_thread(POSSIBLY_UNUSED(void *v))
{
	int ret;
	sem_t sem;
	struct srange_locker lk;

	ret = sem_init(&sem, 0, 0);
	if (ret)
		return (void*)(uintptr_t)ret;
	memset(&lk, 0, sizeof(lk));
	lk.sem = &sem;
	lk.num_range = 1;
	lk.range[0].start = "/foo/baz/";
	lk.range[0].end = "/foo/baz0";
	sem_post(&g_test2_parent_sem);
	ret = srange_lock(g_test2_tracker, &lk);
	if (ret)
		goto done;
	g_test2_thread_got_range = 1;
	sem_post(&g_test2_parent_sem);
	srange_unlock(g_test2_tracker, &lk);
done:
	sem_destroy(&sem);
	return (void*)(uintptr_t)ret;
}

static int test2(void)
{
	pthread_t thread;
	void *rv;
	int ret;
	struct srange_locker lk;
	sem_t sem;

	g_test2_tracker = srange_tracker_init(SRANGE_LOCK_UNIT_MAX_LOCKERS);
	EXPECT_NOT_ERRPTR(g_test2_tracker);
	EXPECT_ZERO(sem_init(&g_test2_parent_sem, 0, 0));
	EXPECT_ZERO(sem_init(&sem, 0, 0));

	lk.sem = &sem;
	lk.num_range = 1;
	lk.range[0].start = "/foo/";
	lk.range[0].end = "/foo0";
	EXPECT_ZERO(srange_lock(g_test2_tracker, &lk));
	g_test2_thread_got_range = 0;
	pthread_create(&thread, NULL, test2_thread, &sem);
	RETRY_ON_EINTR(ret, sem_wait(&g_test2_parent_sem));
	if (g_test2_thread_got_range) {
		fprintf(stderr, "error: child thread did not wait for lock!\n");
		srange_unlock(g_test2_tracker, &lk);
		ret = -EIO;
		goto done;
	}
	srange_unlock(g_test2_tracker, &lk);
	RETRY_ON_EINTR(ret, sem_wait(&g_test2_parent_sem));
	if (!g_test2_thread_got_range) {
		fprintf(stderr, "error: child thread failed to get lock!\n");
		ret = -EIO;
		goto done;
	}
	ret = pthread_join(thread, &rv);
	if (ret) {
		fprintf(stderr, "error: pthread_join failed!\n");
		goto done;
	}
	ret = (int)(uintptr_t)rv;
	if (ret != 0) {
		fprintf(stderr, "error: child thread failed with error %d\n", ret);
		goto done;
	}

done:
	EXPECT_ZERO(sem_destroy(&g_test2_parent_sem));
	EXPECT_ZERO(sem_destroy(&sem));
	srange_tracker_free(g_test2_tracker);
	return ret;
}

int main(void)
{
	EXPECT_ZERO(simple_test());
	EXPECT_ZERO(test2());

	return EXIT_SUCCESS;
}
