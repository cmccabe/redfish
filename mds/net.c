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
#include "mds/const.h"
#include "mds/delegation.h"
#include "mds/heartbeat.h"
#include "mds/mstor.h"
#include "mds/net.h"
#include "mds/srange_lock.h"
#include "mds/user.h"
#include "msg/bsend.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "msg/recv_pool.h"
#include "msg/types.h"
#include "msg/xdr.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_types.h"
#include "util/packed.h"
#include "util/string.h"
#include "util/terror.h"
#include "util/thread.h"
#include "util/time.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/****************************** constants ********************************/
#define MDS_NET_MSG_DUMP_SZ 16384

#define MDS_NET_REPLICA_TIMEO 60

/****************************** types ********************************/
struct mnrp_tls {
	struct srange_locker lk;
};

/****************************** globals ********************************/
/** recv_pool */
struct recv_pool *g_rpool[RF_ENTITY_TY_NUM];

/** recv_pool thread-local storage */
struct mnrp_tls g_mnrp_tls[RF_ENTITY_TY_NUM];

/** Thread that sends heartbeats */
struct redfish_thread g_mds_send_hb_thread;

/** The metadata store */
struct mstor *g_mstor;

/** User data */
struct udata *g_udata;

/** The metadata server ID for this server */
uint16_t g_mid;

/** The primary metadata server ID */
uint16_t g_pri_mid;

/** Current cluster map */
struct cmap *g_cmap;

/** Lock that protects the cluster map */
pthread_mutex_t g_cmap_lock;

/** MDS messengers */
struct msgr *g_msgr[RF_ENTITY_TY_NUM];

/****************************** utility ********************************/
static void fail_replica_mds(POSSIBLY_UNUSED(int mid))
{
	// TODO: implement
}

static void handle_primary_role(struct recv_pool_thread *rt, struct msg *m,
		int op_ret)
{
	struct daemon_info *di;
	struct mtran *tr;
	int i;

	if (op_ret) {
		/* If the operation failed, we don't have to tell the replicas
		 * to do it.  */
		return;
	}

	/* Since we're the primary, we tell the replicas to perform the
	 * same operation */ 
	msg_addref(m);
	pthread_mutex_lock(&g_cmap_lock);
	for (i = 0; i < g_cmap->num_mds; ++i) {
		if (i == g_mid)
			continue;
		di = &g_cmap->minfo[i];
		if (!di->in)
			continue;
		bsend_add(rt->ctx, g_msgr[RF_ENTITY_TY_MDS], BSF_RESP,
			m, di->ip, di->port[RF_ENTITY_TY_MDS],
			MDS_NET_REPLICA_TIMEO, (void*)(uintptr_t)i);
	}
	bsend_join(rt->ctx);
	for (i = 0; i < bsend_get_num_sent(rt->ctx); ++i) {
		tr = bsend_get_mtran(rt->ctx, i);
		if ((tr->m == NULL) || (IS_ERR(tr->m))) {
			int mid = (int)(uintptr_t)bsend_get_mtran_tag(rt->ctx, i);
			glitch_log("fail_replica_mds(%d)\n", mid);
			fail_replica_mds(mid);
		}
		// TOOD: ... check resp ...
	}
	bsend_reset(rt->ctx);
	pthread_mutex_unlock(&g_cmap_lock);
}

static void handle_replica_role(struct recv_pool_thread *rt, struct mtran *tr,
		struct msg *m, int op_ret)
{
	char *buf;

	if (op_ret == 0)
		return;
	/* Replicas can't fail to perform an operation, or else
	 * they get out of sync with the primary. */
	bsend_std_reply(rt->base.fb, rt->ctx, tr, op_ret);
	buf = malloc(MDS_NET_MSG_DUMP_SZ);
	if (buf)
		dump_msg(m, buf, MDS_NET_MSG_DUMP_SZ);
	else
		buf = "(malloc failed)";
	glitch_log("handle_mds_role: replica failed to apply "
		"operation: error %d: operation:\n%s\n", op_ret, buf);
	abort();
}

static void handle_mds_role(struct recv_pool_thread *rt, struct mtran *tr,
	struct msg *m, int op_ret)
{
	if (g_mid == g_pri_mid) {
		handle_primary_role(rt, m, op_ret);
	}
	else {
		handle_replica_role(rt, tr, m, op_ret);
	}
}

/****************************** operations ********************************/
static int handle_mmm_get_mds_status(struct recv_pool_thread *rt,
		struct mtran *tr, POSSIBLY_UNUSED(const struct msg *m))
{
	struct mmm_mds_status_resp resp;
	struct msg *r;

	resp.mid = g_mid;
	resp.pri_mid = g_pri_mid;
	r = MSG_XDR_ALLOC(mmm_mds_status_resp, &resp);
	if (IS_ERR(r)) {
		return PTR_ERR(r);
	}
	return bsend_reply(rt->base.fb, rt->ctx, tr, r);
}

static int handle_mmm_set_primary_user_group(
	POSSIBLY_UNUSED(struct recv_pool_thread *rt),
	POSSIBLY_UNUSED(struct mtran *tr), POSSIBLY_UNUSED(struct msg *m))
{
	return -ENOSYS;
}

static int handle_mmm_add_user_to_group(
	POSSIBLY_UNUSED(struct recv_pool_thread *rt),
	POSSIBLY_UNUSED(struct mtran *tr), POSSIBLY_UNUSED(struct msg *m))
{
	return -ENOSYS;
}

static int handle_mmm_remove_user_from_group(
	POSSIBLY_UNUSED(struct recv_pool_thread *rt),
	POSSIBLY_UNUSED(struct mtran *tr), POSSIBLY_UNUSED(struct msg *m))
{
	return -ENOSYS;
}

static int handle_mmm_create_file_req(POSSIBLY_UNUSED(struct recv_pool_thread *rt),
	POSSIBLY_UNUSED(struct mtran *tr), POSSIBLY_UNUSED(struct msg *m))
{
	return -ENOSYS;
}

static int handle_mmm_open_file_req(POSSIBLY_UNUSED(struct recv_pool_thread *rt),
	POSSIBLY_UNUSED(struct mtran *tr), POSSIBLY_UNUSED(struct msg *m))
{
	return -ENOSYS;
}

static int handle_mmm_mkdirs_req(struct recv_pool_thread *rt,
		struct mtran *tr, struct msg *m)
{
	int ret;
	struct mmm_mkdirs_req req;
	struct mreq_mkdirs mreq;
	struct mnrp_tls *tls = rt->base.priv;

	ret = MSG_XDR_DECODE(mmm_mkdirs_req, m, &req);
	if (ret)
		return ret;
	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = &tls->lk;
	mreq.base.op = MSTOR_OP_MKDIRS;
	mreq.base.full_path = req.path;
	mreq.base.user_name = req.user;
	mreq.mode = req.mode;
	mreq.ctime = req.ctime;
	ret = mstor_do_operation(g_mstor, (struct mreq*)&mreq);
	handle_mds_role(rt, tr, m, ret);
	ret = 0;
	ret = bsend_std_reply(rt->base.fb, rt->ctx, tr, ret);
	XDR_REQ_FREE(mmm_mkdirs_req, &req);
	return ret;
}

static int handle_mmm_listdir_req(struct recv_pool_thread *rt,
		struct mtran *tr, struct msg *m)
{
	int i, ret;
	struct mmm_listdir_req req;
	struct mmm_listdir_resp resp;
	struct mreq_listdir mreq;
	struct mnrp_tls *tls = rt->base.priv;
	struct rf_lentry *le;
	struct msg *r;

	ret = MSG_XDR_DECODE(mmm_mkdirs_req, m, &req);
	if (ret)
		goto done;
	// TODO: implement partial listdir!
	le = calloc(RF_MAX_FILES_PER_LISTDIR, sizeof(struct rf_lentry)); 
	if (!le) {
		ret = -ENOMEM;
		goto done_free_req;
	}
	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = &tls->lk;
	mreq.base.op = MSTOR_OP_LISTDIR;
	mreq.base.full_path = req.path;
	mreq.base.user_name = req.user;
	mreq.le = le;
	mreq.max_stat = RF_MAX_FILES_PER_LISTDIR;
	ret = mstor_do_operation(g_mstor, (struct mreq*)&mreq);
	if (ret < 0) {
		ret = bsend_std_reply(rt->base.fb, rt->ctx, tr, ret);
		goto done_free_le;
	}
	memset(&resp, 0, sizeof(resp));
	resp.le.le_len = mreq.num_stat;
	resp.le.le_val = le;
	r = MSG_XDR_ALLOC(mmm_listdir_resp, &resp);
	if (IS_ERR(r)) {
		ret = PTR_ERR(r);
		goto done_free_le;
	}
	ret = bsend_reply(rt->base.fb, rt->ctx, tr, r);
done_free_le:
	for (i = 0; i < mreq.num_stat; ++i) {
		XDR_REQ_FREE(rf_lentry, &le[i]);
	}
	free(le);
done_free_req:
	XDR_REQ_FREE(mmm_listdir_req, &req);
done:
	return ret;
}

static int handle_mmm_path_stat_req(struct recv_pool_thread *rt,
		struct mtran *tr, struct msg *m)
{
	int ret;
	struct mmm_path_stat_req req;
	struct mmm_stat_resp resp;
	struct mreq_stat mreq;
	struct mnrp_tls *tls = rt->base.priv;
	struct msg *r;

	ret = MSG_XDR_DECODE(mmm_path_stat_req, m, &req);
	if (ret)
		goto done;
	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = &tls->lk;
	mreq.base.op = MSTOR_OP_STAT;
	mreq.base.full_path = req.path;
	mreq.base.user_name = req.user;
	mreq.stat = &resp.stat;
	ret = mstor_do_operation(g_mstor, (struct mreq*)&mreq);
	if (ret < 0) {
		ret = bsend_std_reply(rt->base.fb, rt->ctx, tr, ret);
		goto done_free_req;
	}
	memset(&resp, 0, sizeof(resp));
	r = MSG_XDR_ALLOC(mmm_stat_resp, &resp);
	if (IS_ERR(r)) {
		ret = PTR_ERR(r);
		goto done_free_req;
	}
	ret = bsend_reply(rt->base.fb, rt->ctx, tr, r);
done_free_req:
	XDR_REQ_FREE(mmm_path_stat_req, &req);
done:
	return ret;
}

static int handle_mmm_nid_stat_req(struct recv_pool_thread *rt,
		struct mtran *tr, struct msg *m)
{
	int ret;
	struct mmm_nid_stat_req req;
	struct mmm_stat_resp resp;
	struct mreq_nid_stat mreq;
	struct mnrp_tls *tls = rt->base.priv;
	struct msg *r;

	ret = MSG_XDR_DECODE(mmm_nid_stat_req, m, &req);
	if (ret)
		goto done;
	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = &tls->lk;
	mreq.base.op = MSTOR_OP_STAT;
	mreq.nid = req.nid;
	mreq.stat = &resp.stat;
	ret = mstor_do_operation(g_mstor, (struct mreq*)&mreq);
	if (ret < 0) {
		ret = bsend_std_reply(rt->base.fb, rt->ctx, tr, ret);
		goto done_free_req;
	}
	memset(&resp, 0, sizeof(resp));
	r = MSG_XDR_ALLOC(mmm_stat_resp, &resp);
	if (IS_ERR(r)) {
		ret = PTR_ERR(r);
		goto done_free_req;
	}
	ret = bsend_reply(rt->base.fb, rt->ctx, tr, r);
done_free_req:
	XDR_REQ_FREE(mmm_nid_stat_req, &req);
done:
	return ret;
}

static int handle_mmm_chmod_req(struct recv_pool_thread *rt,
		struct mtran *tr, struct msg *m)
{
	int ret;
	struct mmm_mkdirs_req req;
	struct mreq_mkdirs mreq;
	struct mnrp_tls *tls = rt->base.priv;

	ret = MSG_XDR_DECODE(mmm_mkdirs_req, m, &req);
	if (ret)
		goto done;
	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = &tls->lk;
	mreq.base.op = MSTOR_OP_CHMOD;
	mreq.base.full_path = req.path;
	mreq.base.user_name = req.user;
	mreq.mode = req.mode;
	ret = mstor_do_operation(g_mstor, (struct mreq*)&mreq);
	if (ret)
		goto done_send_reply;
	handle_mds_role(rt, tr, m, ret);
	ret = 0;
done_send_reply:
	ret = bsend_std_reply(rt->base.fb, rt->ctx, tr, ret);
	XDR_REQ_FREE(mmm_mkdirs_req, &req);
done:
	return ret;
}

static int handle_mmm_chown_req(struct recv_pool_thread *rt,
		struct mtran *tr, struct msg *m)
{
	int ret;
	struct mmm_chown_req req;
	struct mreq_chown mreq;
	struct mnrp_tls *tls = rt->base.priv;

	ret = MSG_XDR_DECODE(mmm_chown_req, m, &req);
	if (ret)
		goto done;
	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = &tls->lk;
	mreq.base.op = MSTOR_OP_CHOWN;
	mreq.base.full_path = req.path;
	mreq.base.user_name = req.user;
	mreq.new_user = req.new_user;
	mreq.new_group = req.new_group;
	ret = mstor_do_operation(g_mstor, (struct mreq*)&mreq);
	if (ret)
		goto done_send_reply;
	handle_mds_role(rt, tr, m, ret);
	ret = 0;
done_send_reply:
	ret = bsend_std_reply(rt->base.fb, rt->ctx, tr, ret);
	XDR_REQ_FREE(mmm_chown_req, &req);
done:
	return ret;
}

static int handle_mmm_utimes_req(struct recv_pool_thread *rt,
		struct mtran *tr, struct msg *m)
{
	int ret;
	struct mmm_utimes_req req;
	struct mreq_utimes mreq;
	struct mnrp_tls *tls = rt->base.priv;

	ret = MSG_XDR_DECODE(mmm_utimes_req, m, &req);
	if (ret)
		goto done;
	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = &tls->lk;
	mreq.base.op = MSTOR_OP_UTIMES;
	mreq.base.full_path = req.path;
	mreq.base.user_name = req.user;
	mreq.new_atime = req.new_atime;
	mreq.new_mtime = req.new_mtime;
	ret = mstor_do_operation(g_mstor, (struct mreq*)&mreq);
	if (ret)
		goto done_send_reply;
	handle_mds_role(rt, tr, m, ret);
	ret = 0;
done_send_reply:
	ret = bsend_std_reply(rt->base.fb, rt->ctx, tr, ret);
	XDR_REQ_FREE(mmm_utimes_req, &req);
done:
	return ret;
}

static int handle_mmm_unlink_req(struct recv_pool_thread *rt,
		struct mtran *tr, struct msg *m)
{
	int ret;
	struct mmm_unlink_req req;
	struct mreq_unlink mreq;
	struct mnrp_tls *tls = rt->base.priv;

	ret = MSG_XDR_DECODE(mmm_unlink_req, m, &req);
	if (ret)
		goto done;
	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = &tls->lk;
	mreq.base.op = MSTOR_OP_UTIMES;
	mreq.base.full_path = req.path;
	mreq.base.user_name = req.user;
	mreq.uop = req.uop;
	ret = mstor_do_operation(g_mstor, (struct mreq*)&mreq);
	if (ret)
		goto done_send_reply;
	handle_mds_role(rt, tr, m, ret);
	ret = 0;
done_send_reply:
	ret = bsend_std_reply(rt->base.fb, rt->ctx, tr, ret);
	XDR_REQ_FREE(mmm_unlink_req, &req);
done:
	return ret;
}

static int handle_mmm_rename_req(struct recv_pool_thread *rt,
		struct mtran *tr, struct msg *m)
{
	int ret;
	struct mmm_rename_req req;
	struct mreq_rename mreq;
	struct mnrp_tls *tls = rt->base.priv;

	ret = MSG_XDR_DECODE(mmm_rename_req, m, &req);
	if (ret)
		goto done;
	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = &tls->lk;
	mreq.base.op = MSTOR_OP_UTIMES;
	mreq.base.full_path = req.src;
	mreq.base.user_name = req.user;
	mreq.dst_path = req.dst;
	ret = mstor_do_operation(g_mstor, (struct mreq*)&mreq);
	if (ret)
		goto done_send_reply;
	handle_mds_role(rt, tr, m, ret);
	ret = 0;
done_send_reply:
	ret = bsend_std_reply(rt->base.fb, rt->ctx, tr, ret);
	XDR_REQ_FREE(mmm_rename_req, &req);
done:
	return ret;
}

static int mds_net_handle_tr(struct recv_pool_thread *rt, struct mtran *tr)
{
	int ret;
	uint16_t ty;
	struct msg *m;
	char ep_buf[128];

	m = tr->m;
	tr->m = NULL;
	ty = unpack_from_be16(&tr->m->ty);
	mtran_ep_to_str(tr, ep_buf, sizeof(ep_buf));
	glitch_log("mds_net_handle_mds_tr: incoming message of type %d "
		"from %s\n", ty, ep_buf);
	switch (ty) {
	case mmm_heartbeat_ty:
		// FIXME: record this
		ret = 0;
		break;
	case mmm_status_req_ty:
		ret = handle_mmm_get_mds_status(rt, tr, m);
		break;
	case mmm_set_primary_user_group_ty:
		ret = handle_mmm_set_primary_user_group(rt, tr, m);
		break;
	case mmm_add_user_to_group_ty:
		ret = handle_mmm_add_user_to_group(rt, tr, m);
		break;
	case mmm_remove_user_from_group_ty:
		ret = handle_mmm_remove_user_from_group(rt, tr, m);
		break;
	case mmm_create_file_req_ty:
		ret = handle_mmm_create_file_req(rt, tr, m);
		break;
	case mmm_open_file_req_ty:
		ret = handle_mmm_open_file_req(rt, tr, m);
		break;
	case mmm_mkdirs_req_ty:
		ret = handle_mmm_mkdirs_req(rt, tr, m);
		break;
	case mmm_listdir_req_ty:
		ret = handle_mmm_listdir_req(rt, tr, m);
		break;
	case mmm_path_stat_req_ty:
		ret = handle_mmm_path_stat_req(rt, tr, m);
		break;
	case mmm_nid_stat_req_ty:
		ret = handle_mmm_nid_stat_req(rt, tr, m);
		break;
	case mmm_chmod_req_ty:
		ret = handle_mmm_chmod_req(rt, tr, m);
		break;
	case mmm_chown_req_ty:
		ret = handle_mmm_chown_req(rt, tr, m);
		break;
	case mmm_utimes_req_ty:
		ret = handle_mmm_utimes_req(rt, tr, m);
		break;
	case mmm_unlink_req_ty:
		ret = handle_mmm_unlink_req(rt, tr, m);
		break;
	case mmm_rename_req_ty:
		ret = handle_mmm_rename_req(rt, tr, m);
		break;
	default:
		glitch_log("mds_net_handle_mds_tr: unhandled message "
			   "type %d\n", ty);
		ret = -ENOSYS;
		break;
	}
	msg_release(m);
	if (ret) {
		glitch_log("mds_net_handle_mds_tr: error %d handling "
			"message type %d from %s\n", ret, ty, ep_buf);
	}
	return 0;
}

/****************************** mntrp_tls ********************************/
static int mnrp_tls_init(struct mnrp_tls *tls)
{
	memset(tls, 0, sizeof(struct mnrp_tls));
	return 0;
}

/****************************** initialization ********************************/
void mds_net_init(POSSIBLY_UNUSED(struct fast_log_buf *fb),
		struct unitaryc *conf, struct mdsc *mdsc, uint16_t mid)
{
	int i, j, ret;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	const char *recv_pool_names[RF_ENTITY_TY_NUM] =
		{ "mds_rpool", "osd_rpool", "cli_rpool" };
	const int recv_pool_nthreads[RF_ENTITY_TY_NUM] =
		{ 16, 16, 8 };
	const int recv_pool_ports[RF_ENTITY_TY_NUM] =
		{ mdsc->mds_port, mdsc->osd_port, mdsc->cli_port };
	struct msgr_conf msgr_conf[RF_ENTITY_TY_NUM] = {
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
			.tcp_teardown_timeo = 300,
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

	g_mid = mid;
	g_pri_mid = 0;
	g_cmap = cmap_from_conf(conf, err, err_len);
	if (err[0]) {
		glitch_log("mds_net_init: failed to create cluster map "
			"from configuration: error %s\n", err);
		abort();
	}
	g_udata = udata_create_default();
	if (IS_ERR(g_udata))
		abort();
	g_mstor = mstor_init(g_fast_log_mgr, mdsc->mc, g_udata);
	if (IS_ERR(g_mstor)) {
		glitch_log("failed to initialize the mstor: error %d\n",
			PTR_ERR(g_mstor));
		abort();
	}
	for (i = 0; i < RF_ENTITY_TY_NUM; ++i) {
		g_msgr[i] = msgr_init(err, err_len, &msgr_conf[i]);
		if (err[0]) {
			glitch_log("osd_net_init: failed to create %s "
				"messenger: error %s\n",
				entity_ty_to_short_str(i), err);
		}
	}
	for (i = 0; i < RF_ENTITY_TY_NUM; ++i) {
		ret = mnrp_tls_init(&g_mnrp_tls[i]);
		if (ret) {
			glitch_log("mds_net_init: failed to initialize TLS "
				"for thread %d: error %d (%s)\n",
				i, ret, terror(ret));
			abort();
		}
	}
	for (i = 0; i < RF_ENTITY_TY_NUM; ++i) {
		g_rpool[i] = recv_pool_init(recv_pool_names[i]);
		if (IS_ERR(g_rpool[i])) {
			ret = PTR_ERR(g_rpool[i]);
			glitch_log("mds_net_init: failed to create "
				"%s: error %d (%s)\n", recv_pool_names[i],
				ret, terror(ret));
			abort();
		}
		for (j = 0; j < recv_pool_nthreads[i]; ++j) {
			ret = recv_pool_thread_create(g_rpool[i],
				g_fast_log_mgr, mds_net_handle_tr, &g_mnrp_tls[i]);
			if (ret) {
				glitch_log("mds_net_init: "
					"recv_pool_thread_create failed with "
					"error %d (%s)\n", ret, terror(ret));
				abort();
			}
		}
		recv_pool_msgr_listen(g_rpool[i], g_msgr[i],
			recv_pool_ports[i], err, err_len);
		if (err[0]) {
			glitch_log("mds_net_msgr_init: failed to listen on "
				"port %d : error %s\n",
				recv_pool_ports[i], err);
			abort();
		}
	}
	for (i = 0; i < RF_ENTITY_TY_NUM; ++i) {
		msgr_start(g_msgr[i], err, err_len);
		if (err[0]) {
			glitch_log("mds_net_msgr_init: failed to initialize "
				"%s messenger: error %s",
				entity_ty_to_short_str(i), err);
			abort();
		}
	}
	ret = redfish_thread_create(g_fast_log_mgr, &g_mds_send_hb_thread,
			mds_send_hb_thread, NULL);
	if (ret) {
		glitch_log("mds_net_init: failed to create "
			"mds_send_hb_thread: error %d\n", ret);
		abort();
	}
}

int mds_main_loop(void)
{
	while (1) {
		mt_msleep(100000);
	}
	return 0;
}
