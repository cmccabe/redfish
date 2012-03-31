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
#include "mds/const.h"
#include "msg/bsend.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "msg/recv_pool.h"
#include "msg/types.h"
#include "msg/xdr.h"
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
#include <rpc/xdr.h>
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
		struct mtran *tr, const struct msg *m)
{
	int32_t ret;
	struct mmm_osd_read_req req;
	struct mmm_osd_read_resp resp;
	struct msg *r;
	char *footer;

	ret = MSG_XDR_DECODE(mmm_osd_read_req, m, &req);
	if (ret < 0)
		return ret;
	if (req.len < 0) {
		ret = -EINVAL;
		goto send_resp;
	}
	resp.flags = 0;
	r = msg_xdr_extalloc(mmm_osd_read_resp_ty,
		(xdrproc_t)xdr_mmm_osd_read_resp, &resp, req.len,
		(void**)&footer);
	if (IS_ERR(r)) {
		ret = PTR_ERR(r);
		goto send_resp;
	}
	ret = ostor_read(g_ostor, rt->base.fb, req.cid, req.start,
		footer, req.len);
	if (ret < 0) {
		msg_release(r);
		goto send_resp;
	}
	r = msg_shrink(r, req.len - ret);
	ret = 0;

send_resp:
	if (ret)
		ret = bsend_std_reply(rt->base.fb, rt->ctx, tr, ret);
	else
		ret = bsend_reply(rt->base.fb, rt->ctx, tr, r);
	XDR_REQ_FREE(mmm_osd_read_req, &req);
	return ret;
}

static int handle_mmm_osd_hflush_req(struct recv_pool_thread *rt,
		struct mtran *tr, const struct msg *m)
{
	int32_t dlen, ret;
	struct mmm_osd_hflush_req req;
	const char *footer;

	dlen = msg_xdr_extdecode((xdrproc_t)xdr_mmm_osd_hflush_req,
		m, &req, (const void**)&footer);
	if (dlen < 0) {
		return dlen;
	}
	// TODO: check flags
	ret = ostor_write(g_ostor, rt->base.fb, req.cid, footer, dlen);
	if (ret) {
		goto send_resp;
	}
send_resp:
	ret = bsend_std_reply(rt->base.fb, rt->ctx, tr, ret);
	XDR_REQ_FREE(mmm_osd_hflush_req, &req);
	return ret;
}

static int handle_mmm_osd_chunkrep_req(struct recv_pool_thread *rt,
		struct mtran *tr, const struct msg *m)
{
	int i, j, ret, num_cid;
	struct mmm_osd_chunkrep_req req;
	struct mmm_osd_chunkrep_resp resp;
	struct msg *r;
	uint64_t cid, *cids, *bad;
	int32_t dlen;

	dlen = msg_xdr_extdecode((xdrproc_t)xdr_mmm_osd_chunkrep_req,
		m, &req, (const void**)&cids);
	if (dlen < 0)
		return dlen;
	memset(&resp, 0, sizeof(resp));
	resp.flags = 0;
	r = msg_xdr_extalloc(mmm_osd_chunkrep_resp_ty,
		(xdrproc_t)xdr_mmm_osd_chunkrep_resp, &resp, dlen,
		(void**)&bad);
	if (IS_ERR(r)) {
		ret = PTR_ERR(r);
		goto send_resp;
	}
	num_cid = dlen / sizeof(uint64_t);
	for (i = 0, j = 0; i < num_cid; ++i) {
		cid = unpack_from_be64(&cids[i]);
		ret = ostor_verify(g_ostor, rt->base.fb, cid);
		if (!ret)
			continue;
		pack_to_be64(&bad[j++], cid);
	}
	r = msg_shrink(r, (dlen - j) * sizeof(uint64_t));
	ret = 0;
send_resp:
	if (ret)
		ret = bsend_std_reply(rt->base.fb, rt->ctx, tr, ret);
	else
		ret = bsend_reply(rt->base.fb, rt->ctx, tr, r);
	XDR_REQ_FREE(mmm_osd_chunkrep_req, &req);
	return ret;
}

static int handle_mmm_osd_unlink_req(struct recv_pool_thread *rt,
		struct mtran *tr, const struct msg *m)
{
	struct mmm_osd_unlink_req req;
	int ret;

	ret = MSG_XDR_DECODE(mmm_osd_unlink_req, m, &req);
	if (ret < 0)
		return ret;
	ret = ostor_unlink(g_ostor, rt->base.fb, req.cid);
	ret = bsend_std_reply(rt->base.fb, rt->ctx, tr, ret);
	XDR_REQ_FREE(mmm_osd_unlink_req, &req);
	return ret;
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
	struct msg *m;
	char ep_buf[128];
	
	m = tr->m;
	tr->m = NULL;
	ty = unpack_from_be16(&m->ty);
	mtran_ep_to_str(tr, ep_buf, sizeof(ep_buf));
	glitch_log("osd_net_handle_tr: incoming message of type %d from %s\n",
		ty, ep_buf); // TODO: make optional
	switch (ty) {
	case mmm_osd_read_req_ty:
		ret = handle_mmm_get_osd_read_req(rt, tr, m);
		break;
	case mmm_osd_hflush_req_ty:
		ret = handle_mmm_osd_hflush_req(rt, tr, m);
		break;
	case mmm_osd_chunkrep_req_ty:
		ret = handle_mmm_osd_chunkrep_req(rt, tr, m);
		break;
	case mmm_osd_unlink_req_ty:
		ret = handle_mmm_osd_unlink_req(rt, tr, m);
		break;
	default:
		glitch_log("osd_net_handle_mds_tr: unhandled message "
			   "type %d from %s\n", ty, ep_buf);
		mtran_free(tr);
		ret = -ENOSYS;
		break;
	}
	msg_release(m);
	if (ret) {
		glitch_log("osd_net_handle_mds_tr: error %d handling "
			   "message type %d from %s\n", ret, ty, ep_buf);
	}
	return 0;
}

static int osd_send_hb_thread(struct redfish_thread *rt)
{
	struct mmm_heartbeat resp;
	struct msg *r;
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
	resp.ty = RF_ENTITY_TY_OSD;
	resp.id = g_oid;
	r = MSG_XDR_ALLOC(mmm_resp, &resp);
	if (IS_ERR(r)) {
		abort();
	}
	while (1) {
		glitch_log("osd_send_hb_thread: sending...\n");
		until = mt_time() + OSD_HB_SEND_IVAL;
		for (i = 0; i < g_cmap->num_mds; ++i) {
			di = &g_cmap->minfo[i];
			if (!di->in)
				continue;
			msg_addref(r);
			bsend_add(ctx, g_msgr[RF_ENTITY_TY_MDS], 0, r,
				di->ip, di->port[RF_ENTITY_TY_OSD], 2);
		}
		bsend_join(ctx);
		bsend_reset(ctx);
		mt_sleep_until(until);
	}
	msg_release(r);
	bsend_free(ctx);
	return 0;
}

void osd_net_init(struct unitaryc *conf, uint16_t oid,
		char *err, size_t err_len)
{
	const struct osdc *osdc = unitaryc_lookup_osdc(conf, oid);
	struct msgr_conf oconf[RF_ENTITY_TY_NUM] = {
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
		g_msgr[i] = msgr_init(err, err_len, &oconf[i]);
		if (err[0]) {
			glitch_log("osd_net_init: failed to create %s "
				"messenger: error %s\n",
				entity_ty_to_short_str(i), err);
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
				entity_ty_to_short_str(i), err);
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
