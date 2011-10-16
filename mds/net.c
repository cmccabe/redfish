/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/fast_log.h"
#include "core/fast_log_types.h"
#include "core/glitch_log.h"
#include "util/compiler.h"
#include "util/string.h"

#include <errno.h>
#include <ev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define MAX_MDS_TRANS_THREADS 4
//
//#define MAX_MDS_TRANS_PER_THREAD 4096
//
///** represents the state of an mds transaction */
//enum mds_trans_state
//{
//};
//
///** represents an mds transaction */
//struct mds_trans
//{
//	enum mds_trans_state state;
//};
//
//
//struct mds_trans_thread
//{
//	/** current number of transactions */
//	int num_trans;
//
//	/** array of pointers to MDS transactions */
//	struct mds_trans *trans[MAX_MDS_TRANS_PER_THREAD];
//};
//
//static int g_num_mds_trans_threads = 0;
//
//static struct mds_trans_thread g_mds_trans_threads[MAX_MDS_TRANS_THREADS];
//
//void *do_mds_trans_thread(void *v)
//{
//	struct mds_trans_thread *me = (struct mds_trans_thread*)v;
//
//	while (1) {
//
//	}
//};
//
//void shutdown_mds_net(void)
//{
//	int i;
//
//	for (i = 0; i < g_num_mds_trans_threads; ++i) {
//		...
//	}
//	g_num_mds_trans_threads = 0;
//}
//
//int init_mds_net(void)
//{
//	int i;
//
//	g_num_mds_trans_threads = 0;
//	memset(g_mds_trans_threads, 0, sizeof(g_mds_trans_threads));
//
//	for (i = 0; i < MAX_MDS_TRANS_THREADS; ++i) {
//		struct mds_trans_thread *me = &g_mds_trans_threads[i];
//		me = calloc(1, sizeof(struct mds_trans_thread));
//		if (!me) {
//			shutdown_mds_net();
//			return -ENOMEM;
//		}
//		++g_num_mds_trans_threads;
//	}
//}

static void mds_graceful_shutdown(struct ev_loop *loop, struct ev_signal *w,
				POSSIBLY_UNUSED(int revents))
{
	glitch_log("gracefully shutting down on signal %d\n", w->signum);
	ev_unloop(loop, EVUNLOOP_ALL);
}

int mds_main_loop(void)
{
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

//	ret = init_mds_net();
//	if (ret) {
//		glitch_log("init_mds_net returned error %d\n", ret);
//		return ret;
//	}
//
//	ev_io_init();
//
//

	while (1) {
		ev_loop(default_loop, 0);
	}
	return 0;
}
