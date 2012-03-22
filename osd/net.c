/*
 * Copyright 2012 the Redfish authors
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
#include "mds/limits.h"
#include "msg/bsend.h"
#include "msg/mds.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "msg/osd.h"
#include "msg/recv_pool.h"
#include "osd/net.h"
#include "osd/ostor.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_types.h"
#include "util/packed.h"
#include "util/string.h"
#include "util/thread.h"
#include "util/time.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OSD_NET_MDS_THREADS 4

#define OSD_NET_IO_THREADS 16

#define OSD_REPLY_TIMEO 30

#define OSD_HB_SEND_IVAL 3

/** recv_pool for doing I/O operations for clients and other OSDs */
static struct recv_pool *g_io_rpool;

/** recv_pool for talking to metadata servers */
static struct recv_pool *g_mds_rpool;

/** This OSD's ID */
static uint32_t g_oid;

/** The cluster map */
static struct cmap *g_cmap;

/** The object store */
static struct ostor *g_ostor;

/** OSD messengers */
static struct msgr *g_msgr[RF_ENTITY_TY_NUM];

/** Thread which sends heartbeat messages */
static struct redfish_thread g_osd_send_hb_thread;

static int handle_mmm_get_osd_read_req(struct recv_pool_thread *rt,
				struct mtran *tr)
{
	struct mmm_osd_read_req *m = (struct mmm_osd_read_req*)tr->m;
	struct mmm_resp *resp;
	struct mmm_osd_read_resp *rr;
	uint64_t cid, start;
	uint32_t dlen;
	int32_t ret;

	cid = unpack_from_be64(&m->cid);
	start = unpack_from_be64(&m->start);
	dlen = unpack_from_be32(&m->len);
	msg_release(tr->m);
	if (dlen > MMM_OSD_MAX_IO_SIZE) {
		ret = -EINVAL;
		goto error_resp;
	}
	rr = calloc_msg(MMM_OSD_READ_RESP,
		sizeof(struct mmm_osd_read_resp) + dlen);
	if (!rr) {
		ret = -ENOMEM;
		goto error_resp;
	}
	ret = ostor_read(g_ostor, rt->base.fb, cid, start, rr->data, dlen);
	if (ret < 0) {
		goto error_resp;
	}
	rr = msg_shrink(rr, sizeof(struct mmm_osd_read_req) + ret);
	ret = bsend_add_tr_or_free(rt->ctx, tr->priv, 0, (struct msg*)rr, tr,
		OSD_REPLY_TIMEO);
	if (ret)
		return ret;
	return 0;

error_resp:
	resp = resp_alloc(ret);
	if (!resp)
		return -ENOMEM;
	ret = bsend_add_tr_or_free(rt->ctx, tr->priv, 0, (struct msg*)resp, tr,
		OSD_REPLY_TIMEO);
	if (ret)
		return ret;
	return 0;
}

static int handle_mmm_osd_hflush_req(struct recv_pool_thread *rt,
				struct mtran *tr)
{
	struct mmm_osd_hflush_req *m = (struct mmm_osd_hflush_req *)tr->m;
	struct mmm_resp *resp;
	uint64_t cid;
	uint32_t dlen;
	int32_t ret;

	cid = unpack_from_be64(&m->cid);
	dlen = unpack_from_be32(&m->base.len);
	dlen -= sizeof(struct mmm_osd_hflush_req);
	if (dlen > MMM_OSD_MAX_IO_SIZE) {
		ret = -EINVAL;
		goto send_resp;
	}
	// TODO: honor MMM_HFLUSH_FLAG_SYNC
	ret = ostor_write(g_ostor, rt->base.fb, cid, m->data, dlen);
	if (ret) {
		goto send_resp;
	}
send_resp:
	msg_release(tr->m);
	resp = resp_alloc(ret);
	if (!resp)
		return -ENOMEM;
	ret = bsend_add_tr_or_free(rt->ctx, tr->priv, 0, (struct msg*)resp, tr,
		OSD_REPLY_TIMEO);
	if (ret)
		return ret;
	return 0;
}

/** Handle an incoming message.
 *
 * Notes:
 * - tr->priv contains a pointer to the messenger that originally sent the
 * request
 * - bsend_add_tr_or_free takes care of freeing the transactor for us.
 */
static int osd_net_handle_tr(struct recv_pool_thread *rt, struct mtran *tr)
{
	int ret;
	uint16_t ty;

	ty = unpack_from_be16(&tr->m->ty);
	glitch_log("osd_net_handle_tr: incoming message of type %d\n", ty);
	switch (ty) {
	case MMM_OSD_READ_REQ:
		ret = handle_mmm_get_osd_read_req(rt, tr);
		break;
	case MMM_OSD_HFLUSH_REQ:
		ret = handle_mmm_osd_hflush_req(rt, tr);
		break;
	default:
		glitch_log("osd_net_handle_mds_tr: unhandled message "
			   "type %d\n", ty);
		mtran_free(tr);
		ret = 0;
		break;
	}
	if (ret) {
		glitch_log("osd_net_handle_mds_tr: error %d handling "
			   "message type %d\n", ret, ty);
	}
	return 0;
}

static int osd_send_hb_thread(struct redfish_thread *rt)
{
	struct mmm_heartbeat *m;
	struct daemon_info *di;
	struct bsend *ctx;
	int i;
	time_t until;

	ctx = bsend_init(rt->fb, RF_MAX_MDS);
	if (IS_ERR(ctx)) {
		glitch_log("osd_send_hb_thread: failed to allocate "
			"an RPC context for the heartbeat thread: "
			"error %d\n", PTR_ERR(ctx));
		abort();
	}
	while (1) {
		glitch_log("osd_send_hb_thread: sending...\n");
		until = mt_time() + OSD_HB_SEND_IVAL;
		for (i = 0; i < g_cmap->num_mds; ++i) {
			di = &g_cmap->minfo[i];
			if (!di->in)
				continue;
			m = calloc_msg(MMM_HEARTBEAT,
				sizeof(struct mmm_heartbeat));
			if (!m) {
				glitch_log("mds_send_hb_thread: failed to "
					"allocate memory for heartbeat "
					"message.\n");
				abort();
			}
			pack_to_8(&m->entity, RF_ENTITY_TY_OSD);
			pack_to_be32(&m->id, g_oid);
			bsend_add(ctx, g_msgr[RF_ENTITY_TY_MDS], 0, (struct msg*)m,
				di->ip, di->port[RF_ENTITY_TY_OSD], 2);
		}
		bsend_join(ctx);
		bsend_reset(ctx);
		mt_sleep_until(until);
	}
	bsend_free(ctx);
	return 0;
}

void osd_net_init(struct unitaryc *conf, uint16_t oid,
		char *err, size_t err_len)
{
	const struct osdc *osdc = unitaryc_lookup_osdc(conf, oid);
	struct msgr_conf mconf[RF_ENTITY_TY_NUM] = {
		{
			.max_conn = 65535,
			.max_tran = 65535,
			.tcp_teardown_timeo = 900,
			.name = "mds_msgr",
			.fl_mgr = g_fast_log_mgr,
		},
		{
			.max_conn = 65535,
			.max_tran = 65535,
			.tcp_teardown_timeo = 900,
			.name = "osd_msgr",
			.fl_mgr = g_fast_log_mgr,
		},
		{
			.max_conn = 65535,
			.max_tran = 65535,
			.tcp_teardown_timeo = 300,
			.name = "cli_msgr",
			.fl_mgr = g_fast_log_mgr,
		},
	};
	int i, ret;

	g_oid = oid;
	g_cmap = cmap_from_conf(conf, err, err_len);
	if (err[0]) {
		glitch_log("osd_net_init: failed to create cluster map "
			"from configuration: error %s\n", err);
		abort();
	}
	g_ostor = ostor_init(osdc->oc);
	if (IS_ERR(g_ostor)) {
		glitch_log("osd_net_init: failed to create the object store: "
			"error %d.\n", PTR_ERR(g_ostor));
		abort();
	}
	for (i = 0; i < RF_ENTITY_TY_NUM; ++i) {
		g_msgr[i] = msgr_init(err, err_len, &mconf[i]);
		if (err[0]) {
			glitch_log("osd_net_init: failed to create %s "
				"messenger: error %s\n",
				fish_entity_ty_to_str(i), err);
		}
	}
	g_mds_rpool = recv_pool_init("mds_rpool");
	if (IS_ERR(g_mds_rpool)) {
		glitch_log("osd_net_init: failed to create mds_rpool: "
			   "error %d\n", PTR_ERR(g_mds_rpool));
		abort();
	}
	g_io_rpool = recv_pool_init("io_rpool");
	if (IS_ERR(g_io_rpool)) {
		glitch_log("osd_net_init: failed to create io_rpool: "
			   "error %d\n", PTR_ERR(g_io_rpool));
		abort();
	}
	for (i = 0; i < OSD_NET_MDS_THREADS; ++i) {
		ret = recv_pool_thread_create(g_mds_rpool, g_fast_log_mgr,
			osd_net_handle_tr, NULL);
		if (ret) {
			glitch_log("osd_net_init: failed to create mds thread: "
				   "error %d\n", ret);
			abort();
		}
	}
	for (i = 0; i < OSD_NET_IO_THREADS; ++i) {
		ret = recv_pool_thread_create(g_io_rpool, g_fast_log_mgr,
			osd_net_handle_tr, NULL);
		if (ret) {
			glitch_log("osd_net_init: failed to create io thread: "
				   "error %d\n", ret);
			abort();
		}
	}
	recv_pool_msgr_listen(g_mds_rpool, g_msgr[RF_ENTITY_TY_MDS],
		osdc->mds_port, err, err_len);
	if (err[0]) {
		glitch_log("osd_net_init: failed to listen on port %d: "
			"error %s\n", osdc->mds_port, err);
		abort();
	}
	recv_pool_msgr_listen(g_io_rpool, g_msgr[RF_ENTITY_TY_OSD],
		osdc->osd_port, err, err_len);
	if (err[0]) {
		glitch_log("osd_net_init: failed to listen on port %d: "
			"error %s\n", osdc->osd_port, err);
		abort();
	}
	recv_pool_msgr_listen(g_io_rpool, g_msgr[RF_ENTITY_TY_CLI],
		osdc->cli_port, err, err_len);
	if (err[0]) {
		glitch_log("osd_net_init: failed to listen on port %d: "
			"error %s\n", osdc->cli_port, err);
		abort();
	}
	for (i = 0; i < RF_ENTITY_TY_NUM; ++i) {
		msgr_start(g_msgr[i], err, err_len);
		if (err[0]) {
			glitch_log("osd_net_init: failed to start %s "
				"messenger: error %s\n",
				fish_entity_ty_to_str(i), err);
			abort();
		}
	}
	ret = redfish_thread_create(g_fast_log_mgr, &g_osd_send_hb_thread,
			osd_send_hb_thread, NULL);
	if (ret) {
		glitch_log("osd_net_init: failed to create "
			"g_osd_send_hb_thread: error %d\n", ret);
		abort();
	}
}

int osd_net_main_loop(void)
{
	while (1) {
		mt_msleep(100000);
	}
	return 0;
}
