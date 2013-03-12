/*
 * vim: ts=8:sw=8:tw=79:noet
 * 
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

#include "client/fishc.h"
#include "client/fishc_internal.h"
#include "client/stub/xattrs.h"
#include "mds/const.h"
#include "util/compiler.h"
#include "util/dir.h"
#include "util/error.h"
#include "util/macro.h"
#include "util/path.h"
#include "util/platform/readdir.h"
#include "util/run_cmd.h"
#include "util/safe_io.h"
#include "util/string.h"
#include "util/username.h"

#include <atomic_ops.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

/****************************** macros ********************************/
#define FAILTHREAD_LONG_SLEEP_MS 500
#define FISHC_RPC_TIMEOUT 30
#define RF_FILE_FLAG_WRITABLE 0x1
#define RF_FILE_FLAG_SHUTDOWN 0x2

/****************************** types ********************************/
/** Represents a Redfish client. */
struct redfish_client {
	/** Client ID.  This will be unique among all redfish_client instances
	 * running on this local computer. */
	uint64_t clid;
	/** User that we connected as */
	const char *user;
	/** Messenger for sending out messages */
	struct msgr *msgr;
	/** Fast log manager */
	struct fast_log_mgr *fl_mgr;
	/** Client-supplied log callback */
	redfish_log_fn_t log_cb;
	/** Client-supplied log context */
	void *log_ctx;
	/** Lock.  Protects pri_mid, cmap, fail, disconnecting, and refcnt */
	pthread_mutex_t lock;
	/** Current primary mds */
	int pri_mid;
	/** Cluster map */
	struct cmap *cmap;
	/** set on mds failure */
	int fail;
	/** If nonzero, the current client is disconnecting */
	int disconnecting;
	/** Condition variable that the failover thread waits on */
	pthread_cond_t need_failover_cond;
	/** The failure thread */
	redfish_thread fail_thread;
	/** rpc context for the failure thread */
	struct bsend *fail_ctx;
	/** Condition variable that the RPC threads wait on */
	pthread_cond_t rpc_cond;
	/** Reference count */
	int refcnt;
};

/** Represents a chunk of a Redfish file */
struct rf_file_chunk {
	/** Chunk ID */
	uint64_t cid;
	/** Number of endpoints */ 
	int num_ep;
	/** Where the chunk is located */
	struct endpoint ep[0];
};

/** Thread-local data for client threads */
struct rf_cli_tls {
	/** Fast log buffer */
	struct fast_log_buf *fb;
	/** Blocking RPC context */
	struct bsend *ctx;
};

/****************************** globals ********************************/
/** Lock that protects g_highest_clid */
static pthread_mutex_t g_highest_clid_lock = PTHREAD_MUTEX_INITIALIZER;

/** Highest client ID yet assigned */
static uint64_t g_highest_clid;

/****************************** chunk ********************************/
/** Copy an rf_file_chunk structure.
 *
 * @param dst		The destination
 * @param src		The source
 */
static void rf_file_chunk_copy(struct rf_file_chunk_prealloc *dst,
		const struct rf_file_chunk *src)
{
	size_t src_len = sizeof(struct rf_file_chunk) +
		(src->num_ep * sizeof(struct endpoint));
	memcpy(dst, src, src_len);
}

/****************************** rpc ********************************/
static struct msg *fishc_do_mds_rpc(struct redfish_client *cli,
		struct rf_cli_tls *tls, struct msg *m)
{
	int ret;
	struct msg *resp;
	uint32_t mds_ip;
	uint16_t mds_addr;
	struct timespec ts;
	struct mtran *tr;

	pthread_mutex_lock(&cli->lock);
	while (1) {
		if (cli->disconnecting)
			return ERR_PTR(EIO);
		if (cli->fail) {
			memset(&ts, 0, sizeof(ts));
			ts.tv_sec = FISHC_RPC_TIMEOUT;
			ret = pthread_cond_timedwait(&cli->rpc_cond,
				&cli->lock, &ts);
			if (ret == ETIMEDOUT)
				return ERR_PTR(EIO);
			if (cli->disconnecting)
				return ERR_PTR(EIO);
		}
		mds_ip = cli->cmap->minfo[mid]->ip;
		mds_port = cli->cmap->minfo[mid]->port;
		pthread_mutex_unlock(&cli->lock);
		msg_addref(m);
		bsend_add(tls->ctx, cli->msgr, BSF_RESP, m, mds_ip, mds_port, NULL);
		if (ret) {
			msg_release(m);
			return ret;
		}
		bsend_join(tls->ctx);
		tr = bsend_get_mtran(tls->ctx, 0);
		if (IS_ERR(tr->m)) {
			bsend_reset(tls->ctx);
			pthread_mutex_lock(&cli->lock);
			cli->fail = 1;
			pthread_cond_signal(&cli->need_failover_cond);
			continue;
		}
	}
	return resp;
}

/*************************** failover thread **************************/
/** Send a GET_MDS_STATUS message to an MDS.
 *
 * @param rt		The fail thread
 * @param mds_ip	The IP of the mds to ask
 * @param mds_port	The port of the mds to ask
 *
 * @return		negative values on a network error.
 * 			the new primary mid otherwise.
 */
static int failthread_ask_about_pri_mid(struct redfish_thread *rt,
			uint32_t mds_ip, uint16_t mds_port)
{
	int ret;
	struct mtran *tr;
	struct mmm_mds_status *resp;
	struct redfish_client *cli;
	uint32_t mds_ip;
	uint16_t mds_port;

	cli = (struct redfish_client*)rt->priv;
	pthread_mutex_unlock(&cli->lock);

	m = calloc_msg(MMM_GET_MDS_STATUS,
		sizeof(struct mmm_get_mds_status));
	if (!m)
		return -ENOMEM;
	ret = bsend_add(cli->fail_ctx, cli->msgr, BSF_RESP, m, mds_ip, mds_port, NULL);
	if (ret) {
		msg_release(m);
		return ret;
	}
	bsend_join(cli->fail_ctx);
	tr = bsend_get_mtran(cli->fail_ctx, 0);
	if (IS_ERR(tr->m)) {
		bsend_reset(cli->fail_ctx);
		return PTR_ERR(tr->m);
	}
	resp = MSG_DYNAMIC_CAST(tr->m, MMM_MDS_STATUS,
		sizeof(struct mmm_mds_status));
	if (!resp) {
		bsend_reset(cli->fail_ctx);
		return -EIO;
	}
	ret = unpack_from_be16(&resp->pri_mid)
	bsend_reset(cli->fail_ctx);
	return ret;
}

/** Runs the failover thread.
 *
 * When the former primary MDS is not responding, this thread has the task of
 * re-establishing communication with some MDS.
 *
 * @param rt		The fail thread
 */
static int failthread_run(struct redfish_thread *rt)
{
	struct redfish_client *cli;
	int old_pri_mid, mid, new_pri_mid;
	struct msg *m;
	uint32_t mds_ip;
	uint16_t mds_port;
	char fb_name[FAST_LOG_BUF_NAME_MAX];

	cli = (struct redfish_client*)rt->priv;
	snprintf(fb_name, sizeof(fb_name), "failthread%d", cli->clid);
	fast_log_set_name(rt->fb, fb_name);
	pthread_mutex_lock(&cli->lock);
	while (1) {
		pthread_cond_wait(&cli->need_failover_cond, &cli->lock);
		if (cli->disconnecting) {
			pthread_mutex_unlock(&cli->lock);
			return 0;
		}
		if (!cli->fail) {
			/* handle spurious wakeups */
			continue;
		}
		old_pri_mid = mid = (cli->pri_mid + 1) % cli->cmap->num_mds;
		while (1) {
			mds_ip = cli->cmap->minfo[mid]->ip;
			mds_port = cli->cmap->minfo[mid]->port;
			pthread_mutex_unlock(&cli->lock);
			new_pri_mid = failthread_ask_about_pri_mid(rt,
						mds_ip, mds_port);
			if (new_pri_mid >= 0) {
				/* If there is a new primary in town, we're
				 * done. */
				if (new_pri_mid != old_pri_mid)
					break;
				/* We can trust the old primary if we just
				 * talked to it.  Otherwise, forget it.
				 */
				if (mid == old_pri_mid)
					break;
				CLIENT_LOG(cli, "failthread: mds %d believes "
					"that %d is still the primary.\n",
					mid, new_pri_mid);
				pthread_mutex_lock(&cli->lock);
				continue;
			}
			else {
				CLIENT_LOG(cli, "failthread: error communicating "
					   "with MDS %d: error %d\n",
					   mid, new_pri_mid);
			}
			if (mid == old_pri_mid) {
				CLIENT_LOG(cli, "failthread: no response from "
					"ex-primary mds %d.  Sleeping for %d "
					"milliseconds\n",
					mid, FAILTHREAD_LONG_SLEEP_MS);
				mt_msleep(FAILTHREAD_LONG_SLEEP_MS);
			}
			pthread_mutex_lock(&cli->lock);
			mid = (mid + 1) % cmap->num_mds;
		}
		if (new_pri_mid == old_pri_mid) {
			CLIENT_LOG(cli, "failthread: primary mds %d appears to "
				   "have recovered.\n", mid);
		}
		else {
			CLIENT_LOG(cli, "failthread: the new primary is %d\n"
				new_pri_mid);
			cli->pri_mid = mid;
		}
		pthread_mutex_lock(&cli->lock);
		cli->fail = 0;
		pthread_cond_broadcast(&cli->rpc_cond);
	}
}

/****************************** utility ********************************/
static struct redfish_block_loc **locate_resp_to_block_loc(
		const struct mmm_locate_resp *lresp)
{
	int i, nblc, ep_len;
	const struct mmm_redfish_block_loc *mlocs;
	const struct endpoint *ep;
	struct redfish_block_loc **blcs;
	struct redfish_block_host *bh;
	char buf[32];

	mlocs = lresp.locs_val;
	nblc = lresp.locs_len;
	blcs = calloc(sizeof(struct redfish_block_loc*), nblc + 1);
	if (!blcs)
		return ERR_PTR(ENOMEM);
	for (i = 0; i < nblc; ++i) {  
		ep_len = mlocs[i].ep_len;
		blcs[i] = calloc(1, sizeof(struct redfish_block_loc) +
			(sizeof(struct redfish_block_host) * ep_len));
		if (!blcs[i]) {
			redfish_free_block_loc(blcs, nblc);
			return -ENOMEM;
		}
		blcs[i].start = mlocs[i].start;
		blcs[i].len = mlocs[i].len;
		blcs[i].nhosts = ep_len
		ep = mlocs[i].ep;
		for (j = 0; j < ep_len; ++j) {
			bh = &blcs[i].hosts[j];
			bh.port = ep.port;
			ipv4_to_str(ep.ip, buf, sizeof(buf));
			bh.hostname = strdup(buf);
			if (!bh.hostname) {
				redfish_free_block_loc(blcs, nblc);
				return -ENOMEM;
			}
		}
	}
	return blcs;
}

static int stat_resp_to_rf_stat(struct rf_stat *stat, struct redfish_stat *osa)
{
	osa->length = stat->length;
	osa->is_dir = (stat->mode_and_type & MMM_STAT_TYPE_DIR);
	osa->repl = stat->man_repl;
	osa->block_sz = stat->block_sz;
	osa->mtime = stat->mtime;
	osa->atime = stat->atime;
	osa->nid = stat->nid;
	osa->mode = stat->mode_and_type & MMM_STAT_MODE_MASK;
	osa->owner = strdup(stat->owner);
	if (!osa->owner)
		return -ENOMEM;
	osa->group = strdup(stat->group);
	if (!osa->group) {
		free(osa->owner);
		return -ENOMEM;
	}
	return 0;
}

/****************************** tls ********************************/
static struct rf_cli_tls *client_alloc_tls(struct redfish_thread *rt,
		struct redfish_cli *cli)
{
	char fl_name[FAST_LOG_BUF_NAME_MAX];
	struct rf_cli_tls *tls;
	void *res;

	tls = calloc(1, sizeof(struct rf_cli_tls));
	if (!tls)
		return ERR_PTR(ENOMEM);
	snprintf(fl_name, sizeof(fl_name), "cli_thread_%d", rt->thread_id);
	tls->fb = fast_log_create(cli->fl_mgr, fl_name);
	if (IS_ERR(tls->fb)) {
		res = tls->fb;
		free(tls);
		return res;
	}
	tls->ctx = bsend_init(tls->fb, 1);
	if (IS_ERR(tls->ctx)) {
		res = tls->ctx;
		fast_log_free(tls->fb);
		free(tls);
		return res;
	}
	return tls;
}

static void client_free_tls(void *v)
{
	struct rf_cli_tls *tls = v;

	if (!tls)
		return;
	fast_log_free(tls->fb);
	bsend_free(tls->ctx);
	free(tls);
}

static struct rf_cli_tls *client_get_tls(struct redfish_thread *rt,
		struct redfish_cli *cli)
{
	struct rf_cli_tls *tls;
	int ret;

	tls = pthread_getspecific(cli->tls_key);
	if (tls)
		return tls;
	tls = client_alloc_tls(rt, cli);
	if (IS_ERR(tls)) {
		CLIENT_LOG(cli, "client_get_tls: failed to allocate "
			   "new TLS: error %d\n", PTR_ERR(tls));
		return tls;
	}
	ret = pthread_setspecific(cli->tls_key, tls);
	if (ret) {
		CLIENT_LOG(cli, "client_get_tls: pthread_setspecific failed "
			   "with error %d\n", ret);
		client_free_tls(tls);
		return ERR_PTR(ret);
	}
	return tls;
}

/****************************** operations ********************************/
struct redfish_client *redfish_connect(const char *conf_path,
	const char *user, redfish_log_fn_t log_cb, void *log_ctx,
	char *err, size_t err_len)
{
	int ret;
	struct redfish_client *cli;
	struct unitaryc *conf;
	struct fast_log_buf *fail_fb;

	if ((err[0]) || (err_len <= 1))
		return NULL;
	conf = parse_unitary_conf_file(conf_path, err, err_len);
	if (err[0])
		goto error;
	cli = calloc(1, struct redfish_client);
	if (!cli) {
		ret = ENOMEM;
		goto error_free_conf;
	}
	cli->fl_mgr = fast_log_mgr_init(g_fast_log_dumpers);
	if (IS_ERR(cli->fl_mgr)) {
		ret = PTR_ERR(cli->fl_mgr);
		snprintf(err, err_len, "fast_log_mgr_init failed with error "
			 "%d: %s", ret, terror(ret));
		goto error_free_cli;
	}
	ret = pthread_key_create(&cli->tls_key, client_free_tls);
	if (ret) {
		snprintf(err, err_len, "pthread_key_create failed with error "
			 "%d: %s", ret, terror(ret));
		goto error_free_fast_log_mgr;
	}
	cli->log_cb = log_cb;
	cli->log_ctx = log_ctx;
	ret = pthread_mutex_init(&cli->lock, NULL);
	if (ret) {
		snprintf(err, err_len, "pthread_mutex_init failed with error "
			 "%d: %s", ret, terror(ret));
		goto error_free_pthread_key;
	}
	cli->cmap = cmap_from_conf(conf, err, err_len);
	if (err[0]) {
		goto error_free_lock;
	}
	ret = pthread_cond_init_mt(&cli->need_failover_cond);
	if (ret) {
		snprintf(err, err_len, "pthread_cond_init_mt failed with "
			"error %d: %s", ret, terror(ret));
		goto error_free_cmap;
	}
	ret = pthread_cond_init_mt(&cli->rpc_cond);
	if (ret) {
		snprintf(err, err_len, "pthread_cond_init_mt failed with "
			"error %d: %s", ret, terror(ret));
		goto error_free_failover_cond;
	}
	pthread_mutex_lock(&g_highest_clid_lock);
	cli->clid = ++g_highest_clid;
	pthread_mutex_unlock(&g_highest_clid_lock);
	cli->pri_mid = 0;
	cli->fail = 0;
	cli->disconnecting = 0;
	cli->refcnt = 1;
	fail_fb = fast_log_create(cli->fl_mgr, "redfish_thread_buf");
	if (IS_ERR(fail_fb)) {
		ret = PTR_ERR(fail_fb);
		snprintf(err, err_len, "fast_log_create failed with "
			"error %d: %s", ret, terror(ret));
		goto error_free_rpc_cond;
	}
	cli->fail_ctx = bsend_init(fail_fb, 1);
	if (IS_ERR(cli->fail_ctx)) {
		ret = PTR_ERR(fail_fb);
		snprintf(err, err_len, "bsend_init failed with "
			"error %d: %s", ret, terror(ret));
		goto error_free_fail_fb;
	}
	ret = redfish_thread_create_with_fb(cli->fail_fb, &cli->fail_thread,
		failthread_run, cli);
	if (ret) {
		snprintf(err, err_len, "redfish_thread_create failed with "
			 "error %d: %s", ret, terror(ret));
		goto ...
	}
	free_unitary_conf_file(conf);
	return cli;

error_free_fail_fb:
	fast_log_free(cli->fail_fb);
error_free_rpc_cond:
	pthread_cond_destroy(&cli->rpc_cond);
error_free_failover_cond:
	pthread_cond_destroy(&cli->need_failover_cond);
error_free_cmap:
	cmap_free(cli->cmap);
error_free_lock:
	pthread_mutex_destroy(&cli->lock);
error_free_pthread_key:
	pthread_key_delete(cli->tls_key);
error_free_fast_log_mgr:
	fast_log_mgr_release(cli->fl_mgr);
error_free_cli:
	free(cli);
error_free_conf:
	free_unitary_conf_file(conf);
error:
	return NULL;
}

int redfish_create(POSSIBLY_UNUSED(struct redfish_client *cli),
	POSSIBLY_UNUSED(const char *path),
	POSSIBLY_UNUSED(int mode), POSSIBLY_UNUSED(uint64_t bufsz),
	POSSIBLY_UNUSED(int repl), POSSIBLY_UNUSED(uint32_t blocksz),
	POSSIBLY_UNUSED(struct redfish_file **ofe))
{
	return -ENOTSUP;
}

int redfish_open(POSSIBLY_UNUSED(struct redfish_client *cli),
	POSSIBLY_UNUSED(const char *path),
	POSSIBLY_UNUSED(struct redfish_file **ofe))
{
	return -ENOTSUP;
}

int redfish_mkdirs(struct redfish_client *cli, int mode, const char *path)
{
	int ret;
	char cpath[RF_PATH_MAX];
	struct mmm_mkdirs_req req;
	struct msg *m, *r;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	ret = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (ret < 0)
		goto done; 
	memset(&req, 0, sizeof(req));
	req.ctime = time(NULL);
	req.mode = mode;
	req.path = cpath;
	req.user = cli->user;
	m = MSG_XDR_ALLOC(mmm_mkdirs_req, &req);
	if (IS_ERR(m)) {
		ret = PTR_ERR(m);
		goto done;
	}
	r = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(r)) {
		ret = PTR_ERR(r);
		goto done_release_m;
	}
	ret = msg_xdr_decode_as_generic(r);
	msg_release(r);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_locate(struct redfish_client *cli,
		const char *path, int64_t start, int64_t len,
		struct redfish_block_loc ***blc)
{
	int ret;
	char cpath[RF_PATH_MAX];
	struct mmm_locate_req req;
	struct mmm_locate_resp resp;
	struct msg *m, *r;
	struct rf_cli_tls *tls;
	struct redfish_block_loc **blcs;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	ret = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (ret < 0)
		goto done; 
	memset(&req, 0, sizeof(req));
	req.path = cpath;
	req.user = cli->user;
	req.start = start;
	req.len = len;
	m = MSG_XDR_ALLOC(mmm_locate_req, &req);
	if (IS_ERR(m)) {
		ret = PTR_ERR(m);
		goto done;
	}
	r = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(r)) {
		ret = PTR_ERR(r);
		goto done_release_m;
	}
	ret = msg_xdr_decode_as_generic(r);
	if (ret > 0)
		goto done_release_r;
	ret = MSG_XDR_DECODE(mmm_locate_resp, r, &resp);
	if (ret < 0) {
		ret = -EIO;
		goto done_release_r;
	}
	blcs = locate_resp_to_block_loc(&resp);
	if (IS_ERR(blcs)) {
		ret = PTR_ERR(blcs);
		goto done_release_resp;
	}
	*blc = blcs;
	ret = 0;
done_release_resp:
	XDR_REQ_FREE(mmm_locate_resp, resp);
done_release_r:
	msg_release(r);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_get_path_status(struct redfish_client *cli, const char *path,
				struct redfish_stat* osa)
{
	int ret;
	char cpath[RF_PATH_MAX];
	struct mmm_path_stat_req req;
	struct mmm_stat_resp resp;
	struct msg *m, *r;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	ret = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (ret < 0)
		goto done; 
	memset(&req, 0, sizeof(req));
	req.path = cpath;
	req.user = cli->user;
	m = MSG_XDR_ALLOC(mmm_path_stat_req, &req);
	if (IS_ERR(m)) {
		ret = PTR_ERR(m);
		goto done;
	}
	r = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(r)) {
		ret = PTR_ERR(r);
		goto done_release_m;
	}
	ret = msg_xdr_decode_as_generic(r);
	if (ret > 0)
		goto done_release_r;
	ret = MSG_XDR_DECODE(mmm_stat_resp, r, &resp);
	if (ret < 0) {
		ret = -EIO;
		goto done_release_r;
	}
	ret = stat_resp_to_rf_stat(&resp.stat, osa);
	if (ret)
		goto done_release_resp;
	ret = 0;
done_release_resp:
	XDR_REQ_FREE(mmm_stat_resp, resp);
done_release_r:
	msg_release(r);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_get_file_status(POSSIBLY_UNUSED(struct redfish_file *ofe),
	POSSIBLY_UNUSED(struct redfish_stat *osa))
{
	return -ENOTSUP;
}

int redfish_list_directory(POSSIBLY_UNUSED(struct redfish_client *cli),
	POSSIBLY_UNUSED(const char *path),
	POSSIBLY_UNUSED(struct redfish_dir_entry** oda))
{
	return -ENOTSUP;
}

int redfish_chmod(struct redfish_client *cli, const char *path, int mode)
{
	int ret;
	char cpath[RF_PATH_MAX];
	struct mmm_chmod_req req;
	struct msg *m, *r;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	ret = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (ret < 0)
		goto done; 
	memset(&req, 0, sizeof(req));
	req.ctime = time(NULL);
	req.mode = mode;
	req.path = cpath;
	req.user = cli->user;
	m = MSG_XDR_ALLOC(mmm_chmod_req, &req);
	if (IS_ERR(m)) {
		ret = PTR_ERR(m);
		goto done;
	}
	r = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(r)) {
		ret = PTR_ERR(r);
		goto done_release_m;
	}
	ret = msg_xdr_decode_as_generic(r);
	msg_release(r);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_chown(struct redfish_client *cli, const char *path,
		const char *owner, const char *group)
{
	int ret;
	char cpath[RF_PATH_MAX];
	struct mmm_chown_req req;
	struct msg *m, *r;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	ret = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (ret < 0)
		goto done; 
	memset(&req, 0, sizeof(req));
	req.path = cpath;
	req.user = cli->user;
	req.new_user = owner;
	req.new_group = group;
	m = MSG_XDR_ALLOC(mmm_chown_req, &req);
	if (IS_ERR(m)) {
		ret = PTR_ERR(m);
		goto done;
	}
	r = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(r)) {
		ret = PTR_ERR(r);
		goto done_release_m;
	}
	ret = msg_xdr_decode_as_generic(r);
	msg_release(r);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_utimes(struct redfish_client *cli, const char *path,
		      uint64_t mtime, uint64_t atime)
{
	int ret;
	char cpath[RF_PATH_MAX];
	struct mmm_utimes_req req;
	struct msg *m, *r;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	ret = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (ret < 0)
		goto done; 
	memset(&req, 0, sizeof(req));
	req.path = cpath;
	req.user = cli->user;
	req.new_atime = atime;
	req.new_mtime = mtime;
	m = MSG_XDR_ALLOC(mmm_utimes_req, &req);
	if (IS_ERR(m)) {
		ret = PTR_ERR(m);
		goto done;
	}
	r = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(r)) {
		ret = PTR_ERR(r);
		goto done_release_m;
	}
	ret = msg_xdr_decode_as_generic(r);
	msg_release(r);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

void redfish_disconnect(struct redfish_client *cli)
{
	void *rval;

	pthread_mutex_lock(&cli->lock);
	cli->disconnecting = 1;
	pthread_cond_broadcast(&cli->need_failover_cond);
	pthread_cond_broadcast(&cli->rpc_cond);
	pthread_mutex_unlock(&cli->lock);
	msgr_shutdown(cli->msgr);
	redfish_thread_join(&cli->fail_thread);
	fast_log_free(cli->fail_fb);
	cli->fail_fb = NULL;
	bsend_free(cli->fail_ctx);
	cli->fail_ctx = NULL;
}

void redfish_release_client(struct redfish_client *cli)
{
	int refcnt;

	pthread_mutex_lock(&cli->lock);
	refcnt = --cli->refcnt;
	pthread_mutex_unlock(&cli->lock);
	if (refcnt > 0)
		return;
	msgr_free(cli->msgr);
	pthread_mutex_destroy(&cli->lock);
	cmap_free(cli->cmap);
	pthread_cond_destroy(&cli->need_failover_cond);
	pthread_cond_destroy(&cli->rpc_cond);
	pthread_key_delete(cli->tls_key);
	fast_log_mgr_release(cli->fl_mgr);
	free(cli);
}

int redfish_read(struct redfish_file *ofe, void *data, int32_t len)
{
	return -ENOTSUP;
}

int32_t redfish_available(struct redfish_file *ofe)
{
	return -ENOTSUP;
}

int redfish_pread(struct redfish_file *ofe, void *data, int32_t len, int64_t off)
{
	return -ENOTSUP;
}

int redfish_write(POSSIBLY_UNUSED(struct redfish_file *ofe),
	POSSIBLY_UNUSED(const void *data), POSSIBLY_UNUSED(int32_t len))
{
}

int redfish_fseek_abs(POSSIBLY_UNUSED(struct redfish_file *ofe),
	POSSIBLY_UNUSED(int64_t off))
{
}

int redfish_fseek_rel(POSSIBLY_UNUSED(struct redfish_file *ofe),
	POSSIBLY_UNUSED(int64_t delta), POSSIBLY_UNUSED(int64_t *out))
{
	return 0;
}

int64_t redfish_ftell(POSSIBLY_UNUSED(struct redfish_file *ofe))
{
	return 0;
}

int redfish_hflush(POSSIBLY_UNUSED(struct redfish_file *ofe))
{
	return 0;
}

int redfish_hsync(POSSIBLY_UNUSED(struct redfish_file *ofe))
{
	return 0;
}

static int redfish_unlink_impl(struct redfish_client *cli, const char *path,
		enum mmm_unlink_op uop)
{
	int ret;
	char cpath[RF_PATH_MAX];
	struct mmm_utimes_req req;
	struct msg *m, *r;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	ret = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (ret < 0)
		goto done; 
	memset(&req, 0, sizeof(req));
	req.path = cpath;
	req.user = cli->user;
	req.new_atime = atime;
	req.new_mtime = mtime;
	m = MSG_XDR_ALLOC(mmm_utimes_req, &req);
	if (IS_ERR(m)) {
		ret = PTR_ERR(m);
		goto done;
	}
	r = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(r)) {
		ret = PTR_ERR(r);
		goto done_release_m;
	}
	ret = msg_xdr_decode_as_generic(r);
	msg_release(r);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_unlink(struct redfish_client *cli, const char *path)
{
	return redfish_unlink_impl(cli, path, MMM_UOP_UNLINK);
}

int redfish_unlink_tree(struct redfish_client *cli, const char *path)
{
	return redfish_unlink_impl(cli, path, MMM_UOP_RMRF);
}

int redfish_rmdir(struct redfish_client *cli, const char *path)
{
	return redfish_unlink_impl(cli, path, MMM_UOP_RMDIR);
}

int redfish_rename(struct redfish_client *cli, const char *src, const char *dst)
{
	int ret;
	char csrc[RF_PATH_MAX], cdst[RF_PATH_MAX];
	struct mmm_rename_req req;
	struct msg *m, *r;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	ret = canonicalize_path2(csrc, RF_PATH_MAX, src);
	if (ret < 0)
		goto done; 
	ret = canonicalize_path2(cdst, RF_PATH_MAX, dst);
	if (ret < 0)
		goto done; 
	memset(&req, 0, sizeof(req));
	req.user = cli->user;
	req.src = csrc;
	req.dst = cdst;
	m = MSG_XDR_ALLOC(mmm_rename_req, &req);
	if (IS_ERR(m)) {
		ret = PTR_ERR(m);
		goto done;
	}
	r = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(r)) {
		ret = PTR_ERR(r);
		goto done_release_m;
	}
	ret = msg_xdr_decode_as_generic(r);
	msg_release(r);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_close(POSSIBLY_UNUSED(struct redfish_file *ofe))
{
	return -ENOTSUP;
}

void redfish_free_file(POSSIBLY_UNUSED(struct redfish_file *ofe))
{
}
