/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "mon/worker.h"
#include "util/compiler.h"
#include "util/test.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum worker_msg_test_ty
{
	WORKER_MSG_TEST_INC_A = 8000,
	WORKER_MSG_TEST_INC_B,
};

struct worker_msg_inc_a
{
	struct worker_msg msg;
	int amt;
};

static pthread_mutex_t consumer1_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_a = 0;
static volatile int g_consumer1_did_shutdown = 0;

static int consumer1_worker(struct worker_msg *m, POSSIBLY_UNUSED(void *data))
{
	struct worker_msg_inc_a *msg;
	if (m->ty != WORKER_MSG_TEST_INC_A)
		return -EINVAL;
	msg = (struct worker_msg_inc_a *)m;
	pthread_mutex_lock(&consumer1_lock);
	g_a += msg->amt;
	pthread_mutex_unlock(&consumer1_lock);
	return 0;
}

static void consumer1_shutdown_fn(POSSIBLY_UNUSED(void *v))
{
	g_consumer1_did_shutdown = 1;
}

#define CONSUMER1_TEST_NUM_WORKERS 20
#define CONSUMER1_TEST_NUM_MSGS_PER_WORKER 10

static int consumer1_test(void)
{
	int expect, cur_a, ret, i;
	struct worker *workers[CONSUMER1_TEST_NUM_WORKERS];
	g_a = 0;
	for (i = 0; i < CONSUMER1_TEST_NUM_WORKERS; ++i) {
		char name[WORKER_NAME_MAX];
		snprintf(name, WORKER_NAME_MAX, "consumer.%03d", i);
		workers[i] = worker_start(name, consumer1_worker,
					  consumer1_shutdown_fn, NULL);
	}

	for (i = 0; i < CONSUMER1_TEST_NUM_WORKERS; ++i) {
		int j;
		for (j = 0; j < CONSUMER1_TEST_NUM_MSGS_PER_WORKER; ++j) {
			struct worker_msg_inc_a *msg =
				calloc(1, sizeof(struct worker_msg_inc_a));
			if (!msg) {
				fprintf(stderr, "calloc failed!\n");
				return -ENOMEM;
			}
			msg->msg.ty = WORKER_MSG_TEST_INC_A;
			msg->amt = j;
			ret = worker_sendmsg(workers[i], msg);
			if (ret) {
				fprintf(stderr, "worker_sendmsg(i=%d, j=%d) "
					"returned %d!\n", i, j, ret);
				free(msg);
				return ret;
			}
		}
	}
	for (i = 0; i < CONSUMER1_TEST_NUM_WORKERS; ++i) {
		EXPECT_ZERO(worker_stop(workers[i]));
	}
	for (i = 0; i < CONSUMER1_TEST_NUM_WORKERS; ++i) {
		EXPECT_ZERO(worker_join(workers[i]));
	}
	expect = 0;
	for (i = 0; i < CONSUMER1_TEST_NUM_WORKERS; ++i) {
		int j;
		for (j = 0; j < CONSUMER1_TEST_NUM_MSGS_PER_WORKER; ++j) {
			expect += j;
		}
	}
	pthread_mutex_lock(&consumer1_lock);
	cur_a = g_a;
	pthread_mutex_unlock(&consumer1_lock);
	if (cur_a != expect) {
		fprintf(stderr, "expected the final value of a to be %d, "
			"but it was %d.\n", expect, cur_a);
		return -EDOM;
	}
	EXPECT_NONZERO(g_consumer1_did_shutdown == 1);
	return 0;
}

static int consumer1_errtest(void)
{
	int ret, i;
	struct worker *workers[CONSUMER1_TEST_NUM_WORKERS];
	g_a = 0;
	for (i = 0; i < CONSUMER1_TEST_NUM_WORKERS; ++i) {
		char name[WORKER_NAME_MAX];
		snprintf(name, WORKER_NAME_MAX, "consumer.%03d", i);
		workers[i] = worker_start(name, consumer1_worker, NULL, NULL);
	}

	for (i = 0; i < CONSUMER1_TEST_NUM_WORKERS; ++i) {
		struct worker_msg_inc_a *msg =
			calloc(1, sizeof(struct worker_msg_inc_a));
		if (!msg) {
			fprintf(stderr, "calloc failed!\n");
			return -ENOMEM;
		}
		msg->msg.ty = WORKER_MSG_TEST_INC_B;
		msg->amt = 123;
		ret = worker_sendmsg(workers[i], msg);
		if (ret) {
			fprintf(stderr, "worker_sendmsg(i=%d) "
				"should have succeeded, but it failed!\n", i);
			free(msg);
			return ret;
		}
	}
	for (i = 0; i < CONSUMER1_TEST_NUM_WORKERS; ++i) {
		worker_join(workers[i]);
	}
	return 0;
}

int main(void)
{
	EXPECT_ZERO(worker_init());
	EXPECT_ZERO(consumer1_test());
	EXPECT_ZERO(consumer1_errtest());

	return EXIT_SUCCESS;
}
