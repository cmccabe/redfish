/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/compiler.h"
#include "util/test.h"
#include "mds/str_range_lock.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

static int simple_test(void)
{
	int ret;
	sem_t sem;
	struct str_range_lock me;
	me.start = "/a/";
	me.end = "/a0";
	me.sem = &sem;
	ret = sem_init(&sem, 0, 0);
	if (ret)
		return ret;
	ret = lock_range(&me);
	if (ret)
		goto done;
	unlock_range(&me);
done:
	sem_destroy(&sem);
	return ret;
}

static volatile int test2_thread_got_range;

static void* test2_thread(void *v)
{
	sem_t *parent_sem = (sem_t*)v;
	int ret;
	sem_t sem;
	struct str_range_lock me;
	me.start = "/foo/baz/";
	me.end = "/foo/baz0";
	me.sem = &sem;
	ret = sem_init(&sem, 0, 0);
	if (ret)
		return (void*)(uintptr_t)ret;
	sem_post(parent_sem);
	ret = lock_range(&me);
	if (ret)
		goto done;
	test2_thread_got_range = 1;
	sem_post(parent_sem);
	unlock_range(&me);
done:
	sem_destroy(&sem);
	return (void*)(uintptr_t)ret;
}

static int test2(void)
{
	pthread_t thread;
	void *rv;
	int ret, rval;
	sem_t sem;
	struct str_range_lock me;
	me.start = "/foo/";
	me.end = "/foo0";
	me.sem = &sem;
	ret = sem_init(&sem, 0, 0);
	if (ret)
		return ret;
	ret = lock_range(&me);
	if (ret)
		goto done;
	test2_thread_got_range = 0;
	pthread_create(&thread, NULL, test2_thread, &sem);
	sem_wait(&sem);
	if (test2_thread_got_range) {
		fprintf(stderr, "error: child thread did not wait for lock!\n");
		unlock_range(&me);
		goto done;
	}
	unlock_range(&me);
	sem_wait(&sem);
	if (!test2_thread_got_range) {
		fprintf(stderr, "error: child thread failed to get lock!\n");
		goto done;
	}
	ret = pthread_join(thread, &rv);
	if (ret) {
		fprintf(stderr, "error: pthread_join failed!\n");
		goto done;
	}
	rval = (int)(uintptr_t)rv;
	if (rval != 0) {
		fprintf(stderr, "error: child thread failed with error %d\n", rval);
		goto done;
	}

done:
	sem_destroy(&sem);
	return ret;
}

int main(void)
{
	int ret;
	ret = init_lock_range_subsystem();
	if (ret)
		return ret;
	ret = simple_test();
	if (ret)
		return ret;
	ret = test2();
	if (ret)
		return ret;

	return EXIT_SUCCESS;
}
