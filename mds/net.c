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

#include "core/glitch_log.h"
#include "util/compiler.h"
#include "util/fast_log.h"
#include "util/fast_log_types.h"
#include "util/string.h"

#include <errno.h>
#include <ev.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MDS_TRANS_THREADS 4

#define MAX_MDS_TRANS_PER_THREAD 4096

/** represents the state of an mds transaction */
enum mds_trans_state
{
	MDS_TRANS_READ_TY,
};

/** represents an mds transaction */
struct mds_trans
{
	enum mds_trans_state state;
};

/** represents a thread handling mds transactions */
struct mds_trans_thread
{
	/** pthread ID */
	pthread_t thread;

	/** current number of transactions */
	int num_trans;

	/** array of pointers to MDS transactions */
	struct mds_trans *trans[MAX_MDS_TRANS_PER_THREAD];

	/** libev event loop */
	struct ev_loop *loop;
};

static int g_num_mds_trans_threads = 0;

static struct mds_trans_thread g_mds_trans_threads[MAX_MDS_TRANS_THREADS];

void *do_mds_trans_thread(void *v)
{
	struct mds_trans_thread *me = (struct mds_trans_thread*)v;

	ev_loop(me->loop, 0);

	return NULL;
};

static void mds_trans_destroy(struct mds_trans *trans)
{
	free(trans);
}

static void free_mds_trans_thread(struct mds_trans_thread *me)
{
	int i;

	for (i = 0; i < me->num_trans; ++i) {
		mds_trans_destroy(me->trans[i]);
	}
	if (me->loop)
		ev_loop_destroy(me->loop);
	free(me);
}

static void shutdown_mds_net(void)
{
	int i;

	for (i = 0; i < g_num_mds_trans_threads; ++i) {
		struct mds_trans_thread *me = &g_mds_trans_threads[i];
		ev_unloop(me->loop, EVUNLOOP_ALL);
	}
	for (i = 0; i < g_num_mds_trans_threads; ++i) {
		struct mds_trans_thread *me = &g_mds_trans_threads[i];
		pthread_join(me->thread, NULL);
		free_mds_trans_thread(me);
	}
	g_num_mds_trans_threads = 0;
}

static int init_mds_net(void)
{
	int i, ret;

	g_num_mds_trans_threads = 0;
	memset(g_mds_trans_threads, 0, sizeof(g_mds_trans_threads));

	for (i = 0; i < MAX_MDS_TRANS_THREADS; ++i) {
		struct mds_trans_thread *me = &g_mds_trans_threads[i];
		me = calloc(1, sizeof(struct mds_trans_thread));
		if (!me) {
			glitch_log("init_mds_net: out of memory!\n");
			shutdown_mds_net();
			return -ENOMEM;
		}
		me->loop = ev_loop_new(0);
		if (!me->loop) {
			free_mds_trans_thread(me);
			glitch_log("init_mds_net: ev_loop_new failed!\n");
			shutdown_mds_net();
			return -ENOTSUP;
		}
		ret = pthread_create(&me->thread, NULL,
					do_mds_trans_thread, me);
		if (ret) {
			free_mds_trans_thread(me);
			glitch_log("init_mds_net: failed to create "
				"mds_trans_thread %d\n", i);
			shutdown_mds_net();
			return ret;
		}
		++g_num_mds_trans_threads;
	}
	return 0;
}

static void mds_graceful_shutdown(struct ev_loop *loop, struct ev_signal *w,
				POSSIBLY_UNUSED(int revents))
{
	ev_signal_stop(loop, w);
	glitch_log("gracefully shutting down on signal %d\n", w->signum);
	ev_unloop(loop, EVUNLOOP_ALL);
}

int mds_main_loop(void)
{
	int ret;
	struct ev_loop *default_loop;
	struct ev_signal w_sigint, w_sigterm;

	glitch_log("starting mds main loop\n");

	default_loop = ev_default_loop(EVFLAG_AUTO);
	if (!default_loop) {
		glitch_log("failed to initialize libev (bad LIBEV_FLAGS?)\n");
		return -EIO;
	}
	ev_signal_init(&w_sigint, mds_graceful_shutdown, SIGINT);
	ev_signal_start(default_loop, &w_sigint);
	ev_signal_init(&w_sigterm, mds_graceful_shutdown, SIGTERM);
	ev_signal_start(default_loop, &w_sigterm);

	ret = init_mds_net();
	if (ret) {
		glitch_log("init_mds_net returned error %d\n", ret);
		return ret;
	}

//	ev_io_init();
//
//
	ev_loop(default_loop, 0);
	return 0;
}
