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

#define REDFISH_MDS_NET_DOT_C

#include "common/cluster_map.h"
#include "common/config/mdsc.h"
#include "common/config/unitaryc.h"
#include "core/glitch_log.h"
#include "core/process_ctx.h"
#include "jorm/jorm_const.h"
#include "mds/delegation.h"
#include "mds/dmap.h"
#include "mds/dslots.h"
#include "mds/heartbeat.h"
#include "mds/limits.h"
#include "mds/net.h"
#include "msg/bsend.h"
#include "msg/mds.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "msg/recv_pool.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_types.h"
#include "util/packed.h"
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

#define DEFAULT_MDS_MDS_PORT 9000
#define MDS_MDS_TCP_TIMEOUT 900

#define DEFAULT_MDS_OSD_PORT 9001
#define MDS_OSD_TCP_TIMEOUT 300

#define DEFAULT_MDS_CLI_PORT 9002
#define MDS_CLI_TCP_TIMEOUT 300

#define NUM_DSLOTS 32

/** Standard timeout used for delegation operations */
#define DGOP_STD_TIMEO 60

/** Maximum number of simultaneous transactors to allow. */
#define RF_MAX_TRAN 16384

/** Maximum number of simultaneous TCP sockets to allow for the OSD messenger. */
#define RF_MAX_OSD_CONN 65536

/** Maximum number of simultaneous TCP sockets to allow for the client messenger. */
#define RF_MAX_CLI_CONN 65536

/** recv_pool listening for connections from other MDSes */
struct recv_pool *g_mds_rpool;

/** recv_pool listening for connections from OSDs */
struct recv_pool *g_osd_rpool;

/** recv_pool listening for connections from clients */
struct recv_pool *g_cli_rpool;

/** Thread that sends heartbeats */
struct redfish_thread g_mds_send_hb_thread;

static void mds_net_msgr_init(struct recv_pool **rpool, struct msgr **msgr,
	const char *thread_pool_name, int max_conn,
	recv_pool_handler_fn_t handler, int nthreads, uint16_t port,
	int tcp_timeo, const char *name)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	int i, ret;

	*msgr = msgr_init(err, err_len, max_conn, RF_MAX_TRAN,
			tcp_timeo, g_fast_log_mgr, name);
	if (err[0]) {
		glitch_log("mds_net_msgr_init: failed to create messenger: "
			"error %s\n", err);
		abort();
	}
	*rpool = recv_pool_init(thread_pool_name);
	if (IS_ERR(*rpool)) {
		glitch_log("mds_net_msgr_init: failed to create recv_pool: "
			"error %d\n", PTR_ERR(*rpool));
		abort();
	}
	for (i = 0; i < nthreads; ++i) {
		ret = recv_pool_thread_create(*rpool, g_fast_log_mgr,
				handler, NULL);
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

static void mds_net_msgrs_start(void)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	msgr_start(g_osd_msgr, err, err_len);
	if (err[0]) {
		glitch_log("mds_net_msgr_init: failed to initialize "
			   "g_osd_msgr: error %s", err);
		abort();
	}
	msgr_start(g_mds_msgr, err, err_len);
	if (err[0]) {
		glitch_log("mds_net_msgr_init: failed to initialize "
			   "g_mds_msgr: error %s", err);
		abort();
	}
	msgr_start(g_cli_msgr, err, err_len);
	if (err[0]) {
		glitch_log("mds_net_msgr_init: failed to initialize "
			   "g_cli_msgr: error %s", err);
		abort();
	}
	glitch_log("mds_net_init: started all messengers.\n");
}

static void mds_net_heartbeat_start(void)
{
	int ret;

	ret = redfish_thread_create(g_fast_log_mgr, &g_mds_send_hb_thread,
			mds_send_hb_thread, NULL);
	if (ret) {
		glitch_log("mds_net_init: failed to create "
			"mds_send_hb_thread: error %d\n", ret);
		abort();
	}
}

static int handle_mmm_get_mds_status(struct recv_pool_thread *rt, struct mtran *tr)
{
	int ret;
	struct mmm_mds_status *m;
	struct bsend *ctx = rt->ctx;

	m = calloc_msg(MMM_MDS_STATUS, sizeof(struct mmm_mds_status));
	if (!m)
		return -ENOMEM;
	pack_to_be16(&m->mid, g_mid);
	pack_to_be16(&m->pri_mid, g_pri_mid);
	ret = bsend_add_tr_or_free(ctx, g_mds_msgr, 0, (struct msg*)m, tr,
				DGOP_STD_TIMEO);
	if (ret)
		return ret;
	ret = bsend_join(ctx);
	if (ret != 1)
		return -EIO;
	bsend_reset(ctx);

	return 0;
}

static int mds_net_handle_mds_tr(struct recv_pool_thread *rt, struct mtran *tr)
{
	int ret;
	uint16_t ty;

	if (IS_ERR(tr->m)) {
		glitch_log("mds_net_handle_mds_tr: got error %d\n",
			PTR_ERR(tr->m));
		return 0;
	}
	ty = unpack_from_be16(&tr->m->ty);
	glitch_log("mds_net_handle_mds_tr: incoming message of type %d\n", ty);
	switch (ty) {
	case MMM_GET_MDS_STATUS:
		ret = handle_mmm_get_mds_status(rt, tr);
		break;
	case MMM_MDS_HEARTBEAT:
		// FIXME: record this
		ret = 0;
		break;
	default:
		glitch_log("mds_net_handle_mds_tr: unhandled message "
			   "type %d\n", ty);
		ret = 0;
		break;
	}
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
	if (ret) {
		glitch_log("mds_net_handle_mds_tr: error handling message of "
			   "type %d: error %d\n", ty, ret);
	}
	return 0;
}

static int mds_net_handle_osd_tr(POSSIBLY_UNUSED(struct recv_pool_thread *rt),
				 POSSIBLY_UNUSED(struct mtran *tr))
{
	// Handle message (something like a switch statement based on type)
	//
	// case MMM_OSD_HEARTBEAT:
	// 	Update the OSD's last contact time
	return 0;
}

static int mds_net_handle_cli_tr(POSSIBLY_UNUSED(struct recv_pool_thread *rt),
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

static void mds_net_root_delegation_setup(void)
{
	int ret, i, primary;
	struct delegation *dg;
	struct daemon_info *di;
	struct dg_mds_info *mi;

	dg = delegation_alloc(RF_ROOT_DGID);
	if (IS_ERR(dg)) {
		glitch_log("mds_net_root_delegation_setup: failed to allocate "
			"root delegation!  Error %d\n", PTR_ERR(dg));
		abort();
	}
	primary = 1;
	for (i = 0; i < g_cmap->num_mds; ++i) {
		di = &g_cmap->minfo[i];
		if (!di->in)
			continue;
		mi = delegation_alloc_mds(dg, i, primary);
		if (IS_ERR(mi))
			abort();
		primary = 0;
		mi->addr = di->ip;
		mi->port = di->port;
	}
	ret = dslots_add(g_dslots, &dg, 1);
	if (ret) {
		glitch_log("mds_net_root_delegation_setup: failed to add "
			"root delegation!  Error %d\n", ret);
		abort();
	}
}

void mds_net_init(POSSIBLY_UNUSED(struct fast_log_buf *fb),
		struct unitaryc *conf, struct mdsc *mconf, uint16_t mid)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	g_mid = mid;
	g_cmap = cmap_from_conf(conf, err, err_len);
	if (err[0]) {
		glitch_log("mds_net_init: failed to create cluster map "
			"from configuration: error %s\n", err);
		abort();
	}
	g_dmap = dmap_alloc();
	if (IS_ERR(g_dmap)) {
		glitch_log("mds_net_init: failed to allocate dmap: "
			"error %d\n", PTR_ERR(g_dmap));
		abort();
	}
	g_dslots = dslots_init(NUM_DSLOTS);
	if (IS_ERR(g_dslots)) {
		glitch_log("mds_net_init: failed to allocate dslots: "
			"error %d\n", PTR_ERR(g_dslots));
		abort();
	}
	mds_net_root_delegation_setup();
	mds_net_msgr_init(&g_mds_rpool, &g_mds_msgr,
		"mds_mds_tpool", RF_MAX_MDS, mds_net_handle_mds_tr,
		DEFAULT_MDS_TR_THREADS,
		((mconf->mds_port == JORM_INVAL_INT) ?
		 	DEFAULT_MDS_MDS_PORT : mconf->mds_port),
		MDS_MDS_TCP_TIMEOUT, "g_mds_msgr");
	mds_net_msgr_init(&g_osd_rpool, &g_osd_msgr,
		"mds_osd_tpool", RF_MAX_OSD_CONN, mds_net_handle_osd_tr,
		DEFAULT_OSD_TR_THREADS,
		((mconf->osd_port == JORM_INVAL_INT) ?
		 	DEFAULT_MDS_OSD_PORT : mconf->osd_port),
		MDS_OSD_TCP_TIMEOUT, "g_osd_msgr");
	mds_net_msgr_init(&g_cli_rpool, &g_cli_msgr,
		"mds_cli_tpool", RF_MAX_CLI_CONN, mds_net_handle_cli_tr,
		DEFAULT_CLI_TR_THREADS,
		((mconf->cli_port == JORM_INVAL_INT) ?
		 	DEFAULT_MDS_CLI_PORT : mconf->cli_port),
		MDS_CLI_TCP_TIMEOUT, "g_cli_msgr");
	mds_net_msgrs_start();
	mds_net_heartbeat_start();
}

int mds_main_loop(void)
{
	while (1) {
		mt_msleep(100000);
	}
	return 0;
}
