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

#include "common/cluster_map.h"
#include "common/config/mdsc.h"
#include "common/config/unitaryc.h"
#include "core/glitch_log.h"
#include "core/process_ctx.h"
#include "jorm/jorm_const.h"
#include "msg/bsend.h"
#include "mds/limits.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "msg/recv_pool.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_types.h"
#include "util/string.h"
#include "util/thread.h"
#include "util/time.h"

#include <errno.h>
#include <ev.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_MDS_TR_THREADS 8

#define DEFAULT_OSD_TR_THREADS 8

#define DEFAULT_CLI_TR_THREADS 8

#define MAX_TR_THREADS 16

#define DEFAULT_MDS_MDS_PORT 9000

#define DEFAULT_MDS_OSD_PORT 9001

#define DEFAULT_MDS_CLI_PORT 9002

/** Maximum number of simultaneous transactors to allow. */
#define RF_MAX_TRAN 16384

/** Maximum number of simultaneous TCP sockets to allow for the OSD messenger. */
#define RF_MAX_OSD_CONN 65536

/** Maximum number of simultaneous TCP sockets to allow for the client messenger. */
#define RF_MAX_CLI_CONN 65536

/** Send timeout period for sending messages to nodes */
#define RF_SEND_TIMEO_PERIOD 20

/** Number of send timeout periods to allow to expire before timing out a
 * connection or transactor */
#define RF_SEND_TIMEO_CNT 3

struct mds_tr_thread {
	struct recv_pool_thread base;
};

struct osd_tr_thread {
	struct recv_pool_thread base;
};

struct cli_tr_thread {
	struct recv_pool_thread base;
};

struct maint_thread {
	struct recv_pool_thread base;
};

/** Current cluster map */
struct cmap *g_cmap;

/** Messenger listening for connections from other MDSes */
struct msgr *g_mds_msgr;
/** recv_pool listening for connections from other MDSes */
struct recv_pool *g_mds_rpool;
/** threads listening for connections from other MDSes */
struct mds_tr_thread g_mds_rts[MAX_TR_THREADS];

/** Messenger listening for connections from OSDs */
struct msgr *g_osd_msgr;
/** recv_pool listening for connections from OSDs */
struct recv_pool *g_osd_rpool;
/** threads listening for connections from OSDs */
struct mds_tr_thread g_osd_rts[MAX_TR_THREADS];

/** Messenger listening for connections from clients */
struct msgr *g_cli_msgr;
/** recv_pool listening for connections from clients */
struct recv_pool *g_cli_rpool;
/** threads listening for connections from clients */
struct mds_tr_thread g_cli_rts[MAX_TR_THREADS];

static void mds_net_msgr_init(struct recv_pool **rpool, struct msgr **msgr,
	struct recv_pool_thread *rts, size_t th_sz,
	struct fast_log_buf *fb, int max_conn,
	recv_pool_handler_fn_t handler, int nthreads, uint16_t port)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	int i, ret;
	struct recv_pool_thread *rt;

	if (nthreads > MAX_TR_THREADS) {
		glitch_log("mds_net_msgr_init: compile-time limit violated: "
			   "nthreads > %d\n", MAX_TR_THREADS);
		abort();
	}
	*msgr = msgr_init(err, err_len, max_conn, RF_MAX_TRAN,
		RF_SEND_TIMEO_PERIOD, RF_SEND_TIMEO_CNT, g_fast_log_mgr);
	if (err[0]) {
		glitch_log("mds_net_msgr_init: failed to create messenger: "
			"error %s\n", err);
		abort();
	}
	*rpool = recv_pool_init(fb);
	if (IS_ERR(*rpool)) {
		glitch_log("mds_net_msgr_init: failed to create recv_pool: "
			"error %d\n", PTR_ERR(*rpool));
		abort();
	}
	for (i = 0; i < nthreads; ++i) {
		rt = (struct recv_pool_thread *)(((char*)rts) + (th_sz * i));
		ret = recv_pool_thread_create(*rpool, g_fast_log_mgr,
				rt, handler, NULL);
		if (ret) {
			glitch_log("mds_net_msgr_init: recv_pool_thread_create"
				"failed with error %d\n", ret);
			abort();
		}
	}
	recv_pool_msgr_listen(*rpool, *msgr, port, err, err_len);
	if (err[0]) {
		glitch_log("mds_net_msgr_init: failed to listen on port %d "
			": error %s\n", port, err);
		abort();
	}
}

static int mds_net_handle_mds_tr(POSSIBLY_UNUSED(struct redfish_thread *rt),
			POSSIBLY_UNUSED(struct mtran *tr))
{
	// Identify source MDS ... based on MDS ID embedded in message
	// Can't use IP address because there might be more than one MDS at that
	// IP (although possibly we should check IP?)

	// Update 'last message received at' for that MDS

	// Handle message (something like a switch statement based on type)
	//
	// case MMM_MDS_HEARTBEAT:
	// 	we're done... updating the last message received is all we
	// 	needed to do.
	// case MMM_MDS_PROPOSE_NEW_CMAP:
	// 	Someone is proposing a new cluster map; check it out.
	// 	When should we reject the new cluster map?  Probably never,
	// 	unless we're an MDS being kicked out.  If that was the case,
	// 	they wouldn't / shouldn't have sent to us, anyway.
	// 	So basically the point here is to send back MDS_ACCEPT_NEW_CMAP
	// 	using our handy-dandy bsend()
	// case MMM_MDS_PROPOSE_NEW_DMAP:
	// 	Take __all__ shard locks.
	// 	If we have a pending reshard that conflicts with the proposal,
	// 		then respond with MMM_MDS_NACK_RESHARD.
	// 	Otherwise, accept the delegation map change and put it into a
	// 		pending delegation map changes data structure.
	// 	Release __all__ shard locks.
	//
	// MDS OPERATIONS from primary:
	//	Look up the relevant shard and make sure that we're a replica for it.
	//		If we're not, send back some kind of error and log it.
	//	Take the relevant shard lock.
	// 	Do whatever the primary did.
	// 		If you can't do it, die.
	// 	Release the relevant shard lock.
	// default:
	// 	Log message and ignore.  Or abort()?
	return 0;
}

static int mds_net_handle_osd_tr(POSSIBLY_UNUSED(struct redfish_thread *rt),
				 POSSIBLY_UNUSED(struct mtran *tr))
{
	// Handle message (something like a switch statement based on type)
	//
	// case MMM_OSD_HEARTBEAT:
	// 	Update the OSD's last contact time
	return 0;
}

static int mds_net_handle_cli_tr(POSSIBLY_UNUSED(struct redfish_thread *rt),
				 POSSIBLY_UNUSED(struct mtran *tr))
{
	// Handle message (something like a switch statement based on type)
	//
	// case MMM_RENAME:
	// 	Is it a cross-delegation rename?  If so, then we need to
	// 	reshard.
	// MDS OPERATION:
	//	Look up the relevant shard and make sure that we're a primary for it.
	//		If we're not, send back MMM_MDS_REVEAL_NEW_DMAP
	//	Take the relevant shard lock.
	//	Try to do the operation that is requested.
	// 		If you can't do it, return a NACK (or similar).
	// 	Release the relevant shard lock.
	// default:
	// 	Log message and ignore.  Or abort()?

	return 0;
}

#if 0
static int mds_net_maintenance_thread(struct redfish_thread *rt)
{
	struct maint_thread *rrt = (struct maint_thread *)rt;

	while (1) {
		// If we are the leader (lowest numbered MDS):
		// 1. Periodically do delegation rebalancing.  There should be a
		// heavy bias towards keeping the subtrees the same to minimize
		// unecessary churn!

		// Dig up some zombie chunks and tell the relevant OSDs to
		// actually delete them.
		sleep(1000);
	}
}
#endif

void mds_net_init(struct fast_log_buf *fb, struct unitaryc *conf,
		struct mdsc *mconf)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	g_cmap = cmap_from_conf(conf, err, err_len);
	if (err[0]) {
		glitch_log("mds_net_init: failed to create cluster map "
			"from configuration: error %s\n", err);
		abort();
	}
	mds_net_msgr_init(&g_mds_rpool, &g_mds_msgr,
		(struct recv_pool_thread *)g_mds_rts, sizeof(g_mds_rts[0]),
		fb, RF_MAX_MDS, mds_net_handle_mds_tr,
		DEFAULT_MDS_TR_THREADS,
		((mconf->mds_port == JORM_INVAL_INT) ?
		 	DEFAULT_MDS_MDS_PORT : mconf->mds_port));
	mds_net_msgr_init(&g_osd_rpool, &g_osd_msgr,
		(struct recv_pool_thread *)g_osd_rts, sizeof(g_osd_rts[0]),
		fb, RF_MAX_OSD_CONN, mds_net_handle_osd_tr,
		DEFAULT_OSD_TR_THREADS,
		((mconf->osd_port == JORM_INVAL_INT) ?
		 	DEFAULT_MDS_OSD_PORT : mconf->osd_port));
	mds_net_msgr_init(&g_cli_rpool, &g_cli_msgr,
		(struct recv_pool_thread *)g_cli_rts, sizeof(g_cli_rts[0]),
		fb, RF_MAX_CLI_CONN, mds_net_handle_cli_tr,
		DEFAULT_CLI_TR_THREADS,
		((mconf->cli_port == JORM_INVAL_INT) ?
		 	DEFAULT_MDS_CLI_PORT : mconf->cli_port));
}

int mds_main_loop(void)
{
	while (1) {
		mt_msleep(100000);
	}
	return 0;
}
