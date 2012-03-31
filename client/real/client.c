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

/** Variant of rf_file_chunk that preallocates space for the maximum number of
 * endpoints. */
struct rf_file_chunk_prealloc {
	struct rf_file_vchunk base;
	struct endpoint ep[RF_MAX_OID];
};

/** Represents a Redfish file */
struct redfish_file {
	/** The canonical path to this file */
	char *cpath;
	/** Length of cpath */
	size_t cpath_len;
	/** Client that this file is associated with */
	struct redfish_client *cli;
	/** Lock which protects num_chunks, chunks */
	pthread_mutex_t lock;
	/** Node ID of this file */
	uint64_t nid;
	/** File flags */
	int flags;
	/** Maximum number of bytes in a chunk */
	uint64_t chunk_len;
	/** Offset within file */
	int64_t file_off;
	/** Total length of the file */
	int64_t file_len;
}

enum rf_wof_chunk_state {
	/** The chunk is uninitialized */
	RF_WOF_CHUNK_UNINIT,
	/** We are in the process of getting a new chunk ID from the MDS */
	RF_WOF_CHUNK_LOADING,
	/** The chunk is ready to be written to */
	RF_WOF_CHUNK_READY,
}

struct redfish_wo_file {
	struct redfish_file base;
	/** Current chunk. */
	struct rf_file_chunk_prealloc ch;
	/** Current write offset within the chunk */
	uint64_t ch_off;
	/** Chunk state */
	rf_wof_chunk_state ch_state;
	/** Condition variable used to wait for a new chunk */ 
	pthread_cond_t ch_cond;
	/** length of the write buffer */
	int32_t wr_buf_len;
	/** Current offset within the write buffer */
	int32_t wr_buf_off;
	/** Write buffer */
	char *wr_buf;
};

struct redfish_ro_file {
	struct redfish_file base;
	/** Length of the chunks array */
	uint32_t cached_ch;
	/** The chunks comprising this file */
	struct rf_file_chunk **chunks;
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
		bsend_add(tls->ctx, cli->msgr, BSF_RESP, m, mds_ip, mds_port);
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
	ret = bsend_add(cli->fail_ctx, cli->msgr, BSF_RESP, m, mds_ip, mds_port);
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

/****************************** util ********************************/
static int mmm_stat_resp_to_redfish_stat(struct redfish_stat *osa,
					 char *, uint32_t off, ... 
			const struct mmm_stat_resp *resp)
{
	int ret;
	uint16_t stat_len;
	uint32_t off;
	char owner[RF_USER_MAX];
	char group[RF_GROUP_MAX];

	stat_len = unpack_from_be16(&resp->stat_len);
	osa->mode = unpack_from_be16(&resp->mode_and_type);
	if (osa->mode & MMM_PACKED_STAT_IS_DIR) {
		osa->mode &= ~MMM_PACKED_STAT_IS_DIR;
		osa->is_dir = 1;
	}
	else {
		osa->is_dir = 0;
	}
	osa->length = unpack_from_be64(&resp->length);
	osa->repl = unpack_from_be64(&resp->man_repl);
	osa->block_sz = unpack_from_be64(&resp->block_sz);
	osa->mtime = unpack_from_be64(&resp->mtime);
	osa->atime = unpack_from_be64(&resp->atime);
	osa->owner = unpack_from_be64(&resp->atime);
	off = offsetof(struct mmm_packed_stat, data);
	ret = unpack_str(resp, &off, stat_len, owner, RF_USER_MAX);
	if (ret)
		return ret;
	ret = unpack_str(resp, &off, stat_len, group, RF_GROUP_MAX);
	if (ret)
		return ret;
	osa->owner = strdup(owner);
	if (!osa->owner)
		return -ENOMEM;
	osa->group = strdup(group);
	if (!osa->group) {
		free(osa->owner);
		return -ENOMEM;
	}
	return 0;
}

/*************************** file operations *****************************/
static void *redfish_file_base_alloc(struct redfish_client *cli,
	const char *cpath, uint64_t nid, uint32_t chunk_len, size_t ofe_len)
{
	int ret;
	struct redfish_file *ofe;

	ofe = calloc(1, ofe_len);
	if (!ofe) {
		ret = ENOMEM;
		goto error;
	}
	ofe->cpath = strdup(cpath);
	if (!ofe->cpath) {
		ret = ENOMEM;
		goto error_free_ofe;
	}
	ofe->cpath_len = strlen(cpath);
	ofe->cli = cli;
	ret = pthread_mutex_init(&ofe->lock, NULL);
	if (ret) {
		ret = ENOMEM;
		goto error_free_cpath;
	}
	ofe->nid = nid;
	ofe->chunk_len = chunk_len;
	ofe->file_off = 0;
	return ofe;

error_free_cpath:
	free(ofe->cpath);
error_free_ofe:
	free(ofe);
error:
	return ERR_PTR(FORCE_POSITIVE(ret));
}

static void redfish_file_base_free(void *base)
{
	struct redfish_file *ofe = (struct redfish_file*)base;

	free(ofe->cpath);
	free(ofe);
}

struct redfish_wo_file *redfish_wo_file_alloc(struct redfish_client *cli,
	uint64_t nid, uint32_t chunk_len, int32_t wr_buf_len,
	const struct rf_file_chunk *chunk)
{
	int ret;
	struct redfish_wo_file *ofl;

	ofl = redfish_file_base_alloc(cli, cpath, nid, chunk_len,
				 sizeof(struct redfish_wo_file));
	if (!ofl) {
		ret = ENOMEM;
		goto error;
	}
	rf_file_chunk_copy(&ofl->ch, chunk);
	ofl->ch_off = 0;
	ofl->ch_state = RF_WOF_CHUNK_READY;
	ret = pthread_cond_init_mt(&ofl->ch_cond);
	if (ret) {
		goto error_free_file_base;
	}
	pthread_cond_init
	ofl->wr_buf_len = wr_buf_len;
	ofl->wr_buf_off = 0;
	ofl->wr_buf = malloc(wr_buf_len);
	if (!ofl->wr_buf) {
		ret = ENOMEM;
		goto error_free_ch_cond;
	}
	pthread_mutex_lock(&cli->lock);
	cli->refcnt++;
	pthread_mutex_unlock(&cli->lock);
	return ofl;

error_free_ch_cond:
	pthread_cond_destroy(&ofl->cond);
error_free_file_base:
	redfish_file_base_free(ofl);
error:
	return ERR_PTR(ret);
}

struct redfish_ro_file *redfish_ro_file_alloc(struct redfish_client *cli,
	uint64_t nid, uint32_t chunk_len)
{
	struct redfish_ro_file *ofl;

	ofl = redfish_file_base_alloc(cli, cpath, nid, chunk_len,
				sizeof(struct redfish_ro_file));
	if (!ofl)
		return ERR_PTR(ENOMEM);
	ofl->base.nid = nid;
	ofl->base.flags = 0;
	ofl->base.chunk_len = chunk_len;
	ofl->cached_ch = 0;
	ofl->chunks = NULL;
	pthread_mutex_lock(&cli->lock);
	cli->refcnt++;
	pthread_mutex_unlock(&cli->lock);
	return ofl;
}

int redfish_create_impl(struct redfish_client *cli, const char *path,
	uint64_t mtime, int mode, uint64_t bufsz, int repl,
	uint32_t blocksz, struct redfish_file **ofe)
{
	int clen, ret;
	char cpath[RF_PATH_MAX];
	struct mmm_create_file_req *m;
	struct mmm_create_file_resp *resp;
	struct mmm_resp *r;
	struct mmm_stat_resp *rstat;
	size_t m_len;
	struct rf_cli_tls *tls;

	if (repl > RF_MAX_OID) {
		ret = -EINVAL;
		goto done;
	}
	if (bufsz < blocksz)
		bufsz = blocksz;
	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	clen = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (clen < 0) {
		ret = clen;
		goto done; 
	}
	m_len = sizeof(struct mmm_create_file_req) + clen + 1;
	m = calloc_msg(MMM_CREATE_FILE_REQ, m_len);
	if (!m) {
		ret = -ENOMEM;
		goto done;
	}
	pack_to_be32(&m->block_sz, blocksz);
	pack_to_be32(&m->mode, mode);
	pack_to_be32(&m->repl, repl);
	pack_to_be32(&m->mtime, mtime);
	off = offsetof(struct mmm_create_file_req, data);
	pack_str(m, &off, m_len, cpath);
	resp = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(resp)) {
		ret = PTR_ERR(resp);
		goto done_release_m;
	}
	... todo: write this ...
	... seems like it will be different for create vs open ...
	...

done_release_resp:
	msg_release(resp);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

static void redfish_wo_file_shutdown(struct redfish_wo_file *ofl)
{
	free(ofl->wr_buf);
}

static void redfish_ro_file_shutdown(struct redfish_ro_file *ofl)
{
	for (i = 0; i < ofl->cached_ch; ++i) {
		free(ofl->chunks[i]);
	}
	free(ofl->chunks);
}

/** This function tears down the internal data structures we use to represent a
 * file.  After this function is called, no operations will be possible on the
 * file.
 *
 * This function does not do any network I/O, try to flush anything, etc.
 * It's just the wrecking crew.
 *
 * Locking: you must hold ofe->lock
 *
 * @param ofe		The Redfish file
 */
static void redfish_file_shutdown(struct redfish_file *ofe)
{
	if (ofe->flags & RF_FILE_FLAG_SHUTDOWN)
		return;
	ofe->flags |= RF_FILE_FLAG_SHUTDOWN;
	if (ofe->flags & RF_FILE_FLAG_WRITABLE)
		redfish_ro_file_shutdown((struct redfish_wo_file *)ofe);
	else
		redfish_ro_file_shutdown((struct redfish_ro_file *)ofe);
	free(ofe->cpath);
	ofe->cpath_len = 0;
}

/*************************** chunk flushing ******************************/
/** Implements hflush
 *
 * Locking note: you must hold ofe->lock when you call this function.  This
 * function will release ofe->lock.
 *
 * @param ofl		The file
 *
 * @return		0 on success; error code otherwise
 * 			On error, the file will be closed.
 */
static int redfish_hflush_impl(struct redfish_wo_file *ofl)
{
	int ready = 0;
	char *fl_buf, *new_buf, abuf[INET_ADDRSTRLEN];
	const char *prefix;
	struct rf_file_chunk_prealloc fl_chunk;
	uint64_t fl_ch_off;

	new_buf = malloc(ofe->chunk_len);
	if (!new_buf) {
		CLIENT_LOG(cli, "redfish_hflush_impl(%s): OOM\n",
			ofl.base->cpath);
		redfish_file_shutdown(ofe);
		pthread_mutex_unlock(&ofl->base.lock);
		return -EIO;
	}
	fl_buf = ofe->buf;
	ofe->buf = new_buf;
	ret = redfish_hflush_make_chunk_ready(ofl);
	if (ret) {
		pthread_mutex_unlock(&ofl->base.lock);
		return -EIO;
	}
	rf_file_chunk_copy(&fl_chunk, ofl->cur_chunk.base);
	fl_ch_off = ofl->ch_off;
	ofl->ch_off = 0;
	/* Now we have a valid chunk, a buffer to flush to it, and the offset
	 * within that buffer to use.  We're done with the file structure
	 * completely at this point.  We don't need anything more from it.  So
	 * we'll release the lock.
	 */
	pthread_mutex_unlock(&ofl->base.lock);
	while (1) {
		ret = redfish_hflush_send_out_chunk(ofl->base.cli, fl_buf,
				fl_ch_off, &fl_chunk.base);
		if (ret == 0)
			break;
		CLIENT_LOG(cli, "redfish_hflush_impl(%s): failed to talk to "
				"any osd!  Tried ");
		prefix = "";
		for (i = 0; i < fl_chunk.base.num_chunks; ++i) {
			ipv4_to_str(ofl.base->ep[i].ip, abuf, sizeof(abuf));
			CLIENT_LOG(cli, "%s%s:%d", prefix, abuf,
				ofl.base->ep[i].port);
			prefix = ", ";
		}
		if (retries <  HFLUSH_IMPL_MAX_RETRIES) {
			CLIENT_LOG(cli, "... Now sleeping.\n");
			mt_msleep(1000);
		}
		else {
			CLIENT_LOG(cli, "... Returning EIO\n");
			break;
		}
	}
	if (ret) {
		pthread_mutex_lock(&ofl->base.lock);
		CLIENT_LOG(cli, "redfish_hflush_impl(%s): failed to talk to "
			   "any osd!  error %d\n", ofl.base->cpath, ret);
		redfish_file_shutdown(ofe);
		pthread_mutex_unlock(&ofl->base.lock);
		return -EIO;
	}
	return 0;
}

/** Get a chunk reeady for us.
 * This may involve doing RPC to the MDS cluster if we don't already have a
 * chunk ready.
 *
 * Locking note: you must hold ofe->lock when you call this function.
 *
 * @param ofl		The file
 *
 * @return		0 on success; error code otherwise
 * 			On error, the file will be closed.
 */
static int redfish_hflush_make_chunk_ready(struct redfish_wo_file *ofl)
{
	int ret;

	while (1) {
		if (ofl->base.flags & RF_FILE_FLAG_SHUTDOWN) {
			/* We may get here if there was an I/O error and we
			 * decided to shutdown the file.  We may also get here
			 * if one user thread closed a file while another was
			 * writing to it.  Under POSIX semantics, we would not
			 * abort outstanding writes when closing a file.
			 * However, we are not implementing POSIX semantics. */
			pthread_mutex_unlock(&ofl->base.lock);
			return -ESHUTDOWN;
		}
		switch (ofl->ch_state) {
		case RF_WOF_CHUNK_UNINIT:
			ofl->ch_state = RF_WOF_CHUNK_LOADING;
			pthread_mutex_unlock(&ofl->base.lock);
			ret = refish_hflush_ask_for_chunk(ofl);
			if (ret) {
				redfish_file_shutdown(&ofl.base);
				return ret;
			}
			return 0;
		case RF_WOF_CHUNK_LOADING:
			/* Wait for another thread to finish getting a new
			 * chunk ID from the MDS. */
			pthread_cond_wait(&ch_cond);
			break;
		case RF_WOF_CHUNK_READY:
			/* Chunk is ready. */
			return 0;
		}
	}
}

/** Ask the MDS cluster for a new chunk in our file.
 *
 * @param ofl		The file
 *
 * @return		0 on success; error code otherwise
 */
static int refish_hflush_ask_for_chunk(struct redfish_wo_file *ofl)
{
	int ret;
	struct mmm_chunkalloc_req *m;
	struct mmm_chunkalloc_resp *rchunk;
	struct mmm_resp *resp;
	size_t m_len;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	m_len = sizeof(struct mmm_chunkalloc_req) + ofl->cpath_len + 1;
	m = calloc_msg(MMM_CHUNKALLOC_REQ, m_len);
	if (!m) {
		ret = -ENOMEM;
		goto done;
	}
	off = offsetof(struct mmm_chunkalloc_req, data);
	pack_str(m, &off, m_len, cpath);
	resp = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(resp)) {
		ret = PTR_ERR(resp);
		goto done_release_m;
	}
	resp = MSG_DYNAMIC_CAST(resp, MMM_RESP, sizeof(struct mmm_resp));
	if (resp) {
		ret = unpack_from_be32(&r->error);
		if (ret == 0)
			ret = -EIO;
		goto done_release_resp;
	}
	rchunk = MSG_DYNAMIC_CAST(resp, MMM_CHUNKALLOC_RESP,
			sizeof(struct mmm_chunkalloc_resp));
	if (!rchunk) {
		ret = -EIO;
		goto done_release_resp;
	}
	/* copy new chunk info */
	ofl->ch.cid = unpack_from_be64(&rchunk->cid);
	ofl->ch.num_ep = unpack_from_8(&rchunk->num_ep);
	for (i = 0; i < of->ch.num_ep; ++i) {
		ofl->ch.ep[i].ip = unpack_from_be32(&rchunk.ep[i]->ip);
		ofl->ch.ep[i].port = unpack_from_be16(&rchunk.ep[i]->port);
	}
	ret = 0;

done_release_resp:
	msg_release(resp);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

/** Flush out a chunk to the object storage servers
 *
 * @param cli		The client
 * @param fl_buf	The buffer to send out
 * @param fl_buf_len	Length of the buffer to send out
 * @param fl_ch_off	Offset of the buffer to send out
 * @param ch		Chunk info
 *
 * @return		0 on success; error code otherwise
 * 			We consider the operation to have succeeded if any
 * 			replica responds with success.
 */
static int redfish_hflush_send_out_chunk(struct redfish_client *cli,
		const char *fl_buf, uint32_t fl_buf_len, uint64_t fl_ch_off,
		const struct rf_file_chunk *ch)
{
	struct mmm_hflush_req *m;
	struct mmm_resp *resp;
	size_t m_len;
	uint32_t off;
	int i, ret, err;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		return PTR_ERR(tls);
	}
	m_len = sizeof(struct mmm_hflush_req) + fl_buf_len;
	m = calloc_msg(MMM_HFLUSH_REQ, m_len);
	if (!m)
		return -ENOMEM;
	pack_to_be64(&m->cid, ch->cid);
	pack_to_be32(&m->off, fl_ch_off);
	pack_to_be32(&m->len, fl_buf_len);
	off = offsetof(struct mmm_hflush_req, data);
	memcpy(((char*)m) + off, fl_buf, fl_buf_len);
	for (i = 0; i < ch->num_ep; ++i) {
		msg_addref(m);
		bsend_add(tls->ctx, cli->msgr, BSF_RESP, m,
			ch->ep[i].ip, ch->ep[i].port);
	}
	bsend_join(tls->ctx);
	msg_release(m);

	ret = -EIO;
	for (i = 0; i < ch->num_ep; ++i) {
		tr = bsend_get_mtran(tls->ctx, 0);
		if (IS_ERR(tr->m)) {
			bsend_reset(tls->ctx);
			return PTR_ERR(tr->m);
		}
		resp = MSG_DYNAMIC_CAST(tr->m, MMM_RESP,
			sizeof(struct mmm_resp));
		if (resp) {
			err = unpack_from_be32(&resp->error);
			if (err == 0)
				ret = 0;
		}
	}
	bsend_reset(tls->ctx);
	return ret;
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

int redfish_create(struct redfish_client *cli, const char *path,
	int mode, uint64_t bufsz, int repl,
	uint32_t blocksz, struct redfish_file **ofe)
{
	uint64_t mtime;
	mtime = time(NULL);
	return redfish_create_impl(cli, mtime, path, mode,
			bufsz, repl, blocksz, ofe);
}

int redfish_open(struct redfish_client *cli, const char *path, struct redfish_file **ofe)
{
	...
		reuse redfish_create_impl somehow
}

int redfish_mkdirs(struct redfish_client *cli, int mode, const char *path)
{
	int clen, ret;
	size_t m_len;
	char cpath[RF_PATH_MAX];
	struct mmm_mkdirs_req *m;
	struct mmm_resp *resp;
	uint64_t mtime;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	clen = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (clen < 0) {
		ret = clen;
		goto done; 
	}
	m_len = sizeof(struct mmm_mkdirs_req) + clen + 1;
	m = calloc_msg(MMM_MKDIRS_REQ, m_len);
	if (!m) {
		ret = -ENOMEM;
		goto done;
	}
	off = offsetof(struct mmm_mkdirs_req, data);
	pack_str(m, &off, m_len, cpath);
	mtime = time(NULL);
	pack_to_be64(&m->mtime, mtime);
	pack_to_be16(&m->mode, mode);
	resp = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(resp)) {
		ret = PTR_ERR(resp);
		goto done_release_m;
	}
	resp = MSG_DYNAMIC_CAST(resp, MMM_RESP, sizeof(struct mmm_resp));
	if (!resp) {
		ret = -EIO;
		goto done_release_resp;
	}
	ret = unpack_from_be32(&nack->error);

done_release_resp:
	msg_release(resp);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_locate(POSSIBLY_UNUSED(struct redfish_client *cli),
		POSSIBLY_UNUSED(const char *path), int64_t start, int64_t len,
		struct redfish_block_loc ***blc)
{
	...
}

int redfish_get_path_status(struct redfish_client *cli, const char *path,
				struct redfish_stat* osa)
{
	int clen, ret;
	char cpath[RF_PATH_MAX];
	struct mmm_path_stat_req *m;
	struct mmm_path_stat_req *resp;
	struct mmm_resp *r;
	struct mmm_stat_resp *rstat;
	size_t m_len;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	clen = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (clen < 0) {
		ret = clen;
		goto done; 
	}
	m_len = sizeof(struct mmm_path_stat_req) + clen + 1;
	m = calloc_msg(MMM_PATH_STAT_REQ, m_len);
	if (!m) {
		ret = -ENOMEM;
		goto done;
	}
	off = offsetof(struct mmm_path_stat_req, data);
	pack_str(m, &off, m_len, cpath);
	resp = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(resp)) {
		ret = PTR_ERR(resp);
		goto done_release_m;
	}
	r = MSG_DYNAMIC_CAST(resp, MMM_RESP, sizeof(struct mmm_resp));
	if (r) {
		ret = unpack_from_be32(&r->error);
		if (ret == 0)
			ret = -EIO;
		goto done_release_resp;
	}
	rstat = MSG_DYNAMIC_CAST(resp, MMM_STAT_RESP,
		sizeof(struct mmm_stat_resp));
	if (rstat) {
		ret = mmm_stat_resp_to_redfish_stat(osa, rstat);
		if (ret) {
			memset(osa, 0, sizeof(struct redfish_stat));
			goto done_release_resp;
		}
	}
	else {
		ret = -EIO;
		goto done_release_resp;
	}
	ret = 0;

done_release_resp:
	msg_release(resp);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_get_file_status(struct redfish_file *ofe, struct redfish_stat *osa)
{
	int ret;
	int64_t file_len;
	struct mmm_path_stat_req *m;
	struct mmm_path_stat_req *resp;
	struct mmm_resp *r;
	struct mmm_stat_resp *rstat;
	size_t m_len;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	m_len = sizeof(struct mmm_nid_stat_req);
	m = calloc_msg(MMM_NID_STAT_REQ, m_len);
	if (!m) {
		ret = -ENOMEM;
		goto done;
	}
	pthread_mutex_lock(&ofe->lock);
	pack_to_be64(&m->nid, ofe->nid);
	file_len = ofe->file_len;
	pthread_mutex_unlock(&ofe->lock);
	resp = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(resp)) {
		ret = PTR_ERR(resp);
		goto done_release_m;
	}
	r = MSG_DYNAMIC_CAST(resp, MMM_RESP, sizeof(struct mmm_resp));
	if (r) {
		ret = unpack_from_be32(&r->error);
		if (ret == 0)
			ret = -EIO;
		goto done_release_resp;
	}
	rstat = MSG_DYNAMIC_CAST(resp, MMM_STAT_RESP,
		sizeof(struct mmm_stat_resp));
	if (rstat) {
		ret = mmm_stat_resp_to_redfish_stat(osa, rstat);
		if (ret) {
			memset(osa, 0, sizeof(struct redfish_stat));
			goto done_release_resp;
		}
		/* If we have more up-to-date information on the length, then
		 * use it. */
		if (file_len > rstat->length) {
			rstat->length = file_len;
		}
	}
	else {
		ret = -EIO;
		goto done_release_resp;
	}
	ret = 0;

done_release_resp:
	msg_release(resp);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_list_directory(struct redfish_client *cli, const char *path,
			      struct redfish_dir_entry** oda)
{
	int i, clen, ret, noda;
	char cpath[RF_PATH_MAX], pcomp[RF_PCOMP_MAX];
	struct mmm_listdir_req *m;
	struct mmm_resp *resp;
	struct mmm_listdir_resp *rlist;
	size_t m_len;
	struct rf_cli_tls *tls;
	struct redfish_dir_entry* zoda;
	uint32_t off;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = FORCE_NEGATIVE(PTR_ERR(tls));
		goto done;
	}
	clen = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (clen < 0) {
		ret = clen;
		goto done; 
	}
	m_len = sizeof(struct mmm_listdir_req) + clen + 1;
	m = calloc_msg(MMM_LISTDIR_REQ, m_len);
	if (!m) {
		ret = -ENOMEM;
		goto done;
	}
	off = offsetof(struct mmm_listdir_req, data);
	pack_str(m, &off, m_len, cpath);
	resp = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(resp)) {
		ret = FORCE_NEGATIVE(PTR_ERR(resp));
		goto done_release_m;
	}
	resp = MSG_DYNAMIC_CAST(resp, MMM_RESP, sizeof(struct mmm_resp));
	if (resp) {
		ret = FORCE_NEGATIVE(unpack_from_be32(&resp->error));
		if (ret == 0)
			ret = -EIO;
		goto done_release_resp;
	}
	rlist = MSG_DYNAMIC_CAST(resp, MMM_LISTDIR_RESP,
		sizeof(struct mmm_listdir_resp));
	if (!rlist) {
		ret = -EIO;
		goto done_release_resp;
	}
	noda = unpack_from_be32(&rlist->num_elem);
	if (noda < 0) {
		ret = -EIO;
		goto done_release_resp;
	}
	zoda = calloc(1, sizeof(struct redfish_dir_entry) * noda);
	if (!zoda) {
		ret = -ENOMEM;
		goto done_release_resp;
	}
	off = offsetof(struct mmm_listdir_resp, data);
	m_len = unpack_from_be32(resp.base->len);
	for (i = 0; i < zoda; ++i) {
		ret = unpack_str(resp, &off, m_len, pcomp, RF_PCOMP_MAX);
		if (ret) {
			ret = FORCE_NEGATIVE(ret);
			goto done_release_resp;
		}
		zoda[i].name = strdup(pcomp);
		if (!zoda[i].name) {
			ret = -ENOMEM;
			goto done_release_resp;
		}
		if (m_len - off < sizeof ... )
			...
			todo: refactor this
		ret = mmm_stat_resp_to_redfish_stat(&zoda[i].stat,
			((struct mmm_stat_resp*)((char*)resp) + off));
		if (ret) {
			ret = FORCE_NEGATIVE(ret);
			goto done_release_resp;
		}
	}
	*oda = zoda;
	zoda = NULL;
	ret = noda;

done_release_resp:
	if (zoda) {
		for (i = 0; i < noda; ++i) {
			free(oda[i].name);
			free(oda[i].stat.owner);
			free(oda[i].stat.group);
		}
		free(zoda);
	}
	msg_release(resp);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_chmod(struct redfish_client *cli, const char *path, int mode)
{
	int clen, ret;
	size_t m_len;
	char cpath[RF_PATH_MAX];
	struct mmm_chmod_req *m;
	struct mmm_resp *resp;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	clen = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (clen < 0) {
		ret = clen;
		goto done; 
	}
	m_len = sizeof(struct mmm_chmod_req) + clen + 1;
	m = calloc_msg(MMM_CHMOD_REQ, m_len);
	if (!m) {
		ret = -ENOMEM;
		goto done;
	}
	off = offsetof(struct mmm_chmod_req, data);
	pack_str(m, &off, m_len, cpath);
	resp = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(resp)) {
		ret = PTR_ERR(resp);
		goto done_release_m;
	}
	resp = MSG_DYNAMIC_CAST(resp, MMM_RESP, sizeof(struct mmm_resp));
	if (!resp) {
		ret = -EIO;
		goto done_release_resp;
	}
	ret = unpack_from_be32(&nack->error);

done_release_resp:
	msg_release(resp);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_chown(struct redfish_client *cli, const char *path,
		  const char *owner, const char *group)
{
	int clen, ret;
	size_t m_len;
	char cpath[RF_PATH_MAX];
	struct mmm_chown_req *m;
	struct mmm_resp *resp;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	if (owner) {
		if (owner[0] == '\0') {
			ret = -EINVAL;
			goto done;
		}
	}
	else {
		owner = "";
	}
	if (group) {
		if (group[0] == '\0') {
			ret = -EINVAL;
			goto done;
		}
	}
	else {
		group = "";
	}
	clen = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (clen < 0) {
		ret = clen;
		goto done; 
	}
	m_len = sizeof(struct mmm_chown_req) + clen + 1 +
		strlen(user) + 1 + strlen(group) + 1;
	m = calloc_msg(MMM_CHOWN_REQ, m_len);
	if (!m) {
		ret = -ENOMEM;
		goto done;
	}
	off = offsetof(struct mmm_chown_req, data);
	pack_str(m, &off, m_len, cpath);
	ret = pack_str(m, &off, m_len, owner ? owner : "");
	if (ret)
		goto done_release_m;
	ret = pack_str(m, &off, m_len, group ? group : "");
	if (ret)
		goto done_release_m;
	resp = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(resp)) {
		ret = PTR_ERR(resp);
		goto done_release_m;
	}
	resp = MSG_DYNAMIC_CAST(resp, MMM_RESP, sizeof(struct mmm_resp));
	if (!resp) {
		ret = -EIO;
		goto done_release_resp;
	}
	ret = unpack_from_be32(&nack->error);

done_release_resp:
	msg_release(resp);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_utimes(struct redfish_client *cli, const char *path,
		      uint64_t mtime, uint64_t atime)
{
	int clen, ret;
	char cpath[RF_PATH_MAX];
	struct mmm_path_stat_req *m;
	struct mmm_resp *resp;
	size_t m_len;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	clen = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (clen < 0) {
		ret = clen;
		goto done; 
	}
	m_len = sizeof(struct mmm_utimes_req) + clen + 1;
	m = calloc_msg(MMM_UTIMES_REQ, m_len);
	if (!m) {
		ret = -ENOMEM;
		goto done;
	}
	pack_to_be64(&m->atime, atime);
	pack_to_be64(&m->mtime, mtime);
	off = offsetof(struct mmm_utimes_req, data);
	pack_str(m, &off, m_len, cpath);
	resp = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(resp)) {
		ret = PTR_ERR(resp);
		goto done_release_m;
	}
	r = MSG_DYNAMIC_CAST(resp, MMM_RESP, sizeof(struct mmm_resp));
	if (!r) {
		ret = -EIO;
		goto done_release_resp;
	}
	ret = unpack_from_be32(&r->error);

done_release_resp:
	msg_release(resp);
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

int redfish_write(struct redfish_file *ofe, const void *data, int32_t len)
{
	int ret;
	int32_t amt;
	struct redfish_wo_file *ofl;

	pthread_mutex_lock(&ofe->lock);
	if (!(ofe->flags & RF_FILE_FLAG_WRITABLE)) {
		pthread_mutex_unlock(&ofe->lock);
		return -EBADF;
	}
	else if (len < 0) {
		pthread_mutex_unlock(&ofe->lock);
		return -EINVAL;
	}
	else if (len == 0) {
		pthread_mutex_unlock(&ofe->lock);
		return 0;
	}
	ofl = (struct redfish_wo_file*)ofe;
	while (1) {
		if (ofl->wr_buf_off + len < ofl->wr_buf_len) {
			memcpy(ofl->wr_buf, data, len);
			pthread_mutex_unlock(&ofe->lock);
			return 0;
		}
		amt = ofl->wr_buf_len - ofl->wr_buf_off;
		memcpy(ofl->wr_buf, data, amt);
		len -= amt;
		data = ((char*)data) + amt;
		/* note: redfish_hflush_impl releases ofe->lock. */
		ret = redfish_hflush_impl(ofl);
		if (ret)
			return ret;
		if (len == 0)
			return 0;
		pthread_mutex_lock(&ofe->lock);
	}
}

int redfish_fseek_abs(struct redfish_file *ofe, int64_t off)
{
	if (off < 0)
		return -EINVAL;
	pthread_mutex_lock(ofe->lock);
	if (ofe->flags & RF_FILE_FLAG_WRITABLE) {
		/* Can't seek in a write-only file */
		pthread_mutex_unlock(ofe->lock);
		return -EINVAL;
	}
	ofe->file_off = off;
	pthread_mutex_unlock(ofe->lock);
	return 0;
}

int redfish_fseek_rel(struct redfish_file *ofe, int64_t delta, int64_t *out)
{
	int64_t off;

	pthread_mutex_lock(ofe->lock);
	if (ofe->flags & RF_FILE_FLAG_WRITABLE) {
		/* Can't seek in a write-only file */
		pthread_mutex_unloch(ofe->lock);
		return -EINVAL;
	}
	off = ofe->file_off;
	off = ((uint64_t)off) + ((uint64_t)delta);
	if (ofe->file_off < 0)
		ofe->file_off = 0;
	if (ofe->file_off > file_len)
		ofe->file_off = file_len;
	*out = ofe->file_off;
	pthread_mutex_unlock(ofe->lock);
	return 0;
}

int64_t redfish_ftell(struct redfish_file *ofe)
{
	int64_t off;

	pthread_mutex_lock(ofe->lock);
	off = ofe->file_off;
	pthread_mutex_unlock(ofe->lock);
	return off;
}

int redfish_hflush(struct redfish_file *ofe)
{
	int ret;

	pthread_mutex_lock(ofe->lock);
	if (!(ofe->flags & RF_FILE_FLAG_WRITABLE)) {
		/* Files open for read can't have any dirty data that needs to
		 * be flushed. */
		pthread_mutex_unloch(ofe->lock);
		return 0;
	}
	ret = redfish_hflush_impl(ofe, 0); /* releases lock */
	return ret;
}

int redfish_hsync(struct redfish_file *ofe)
{
	pthread_mutex_lock(ofe->lock);
	if (!(ofe->flags & RF_FILE_FLAG_WRITABLE)) {
		/* Files open for read can't have any dirty data that needs to
		 * be synced to disk. */
		pthread_mutex_unloch(ofe->lock);
		return 0;
	}
	 /* releases lock */
	ret = redfish_hflush_impl(ofe, MMM_HFLUSH_FLAG_SYNC);
	return 0;
}

int redfish_unlink(struct redfish_client *cli, const char *path)
{
	int clen, ret;
	char cpath[RF_PATH_MAX];
	struct mmm_path_stat_req *m;
	struct mmm_path_stat_req *resp;
	struct mmm_resp *r;
	struct mmm_stat_resp *rstat;
	uint16_t ty;
	size_t m_len;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	clen = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (clen < 0) {
		ret = clen;
		goto done; 
	}
	m_len = sizeof(struct mmm_unlink_req) + clen + 1;
	m = calloc_msg(MMM_UNLINK_REQ, m_len);
	if (!m) {
		ret = -ENOMEM;
		goto done;
	}
	off = offsetof(struct mmm_unlink_req, data);
	pack_str(m, &off, m_len, cpath);
	resp = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(resp)) {
		ret = PTR_ERR(resp);
		goto done_release_m;
	}
	r = MSG_DYNAMIC_CAST(resp, MMM_RESP, sizeof(struct mmm_resp));
	if (!r) {
		ret = -EIO;
		goto done_release_resp;
	}
	ret = unpack_from_be32(&r->error);

done_release_resp:
	msg_release(resp);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

static int redfish_unlink_tree_impl(struct redfish_client *cli,
		const char *path, uint8_t flags)
{
	int clen, ret;
	char cpath[RF_PATH_MAX];
	struct mmm_path_stat_req *m;
	struct mmm_path_stat_req *resp;
	struct mmm_resp *r;
	struct mmm_stat_resp *rstat;
	uint16_t ty;
	size_t m_len;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	clen = canonicalize_path2(cpath, RF_PATH_MAX, path);
	if (clen < 0) {
		ret = clen;
		goto done; 
	}
	m_len = sizeof(struct mmm_unlink_tree_req) + clen + 1;
	m = calloc_msg(MMM_UNLINK_TREE_REQ, m_len);
	if (!m) {
		ret = -ENOMEM;
		goto done;
	}
	pack_to8(m->flags, MMM_UNLINK_TREE_FLAG_POSIX);
	off = offsetof(struct mmm_unlink_req, data);
	pack_str(m, &off, m_len, cpath);
	resp = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(resp)) {
		ret = PTR_ERR(resp);
		goto done_release_m;
	}
	r = MSG_DYNAMIC_CAST(resp, MMM_RESP, sizeof(struct mmm_resp));
	if (!r) {
		ret = -EIO;
		goto done_release_resp;
	}
	ret = unpack_from_be32(&r->error);

done_release_resp:
	msg_release(resp);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_rmdir(struct redfish_client *cli, const char *path)
{
	return redfish_unlink_tree_impl(cli, path, 0);
}

int redfish_unlink_tree(struct redfish_client *cli, const char *path)
{
	return redfish_unlink_tree_impl(cli, path, MMM_UNLINK_TREE_FLAG_POSIX);
}

int redfish_rename(struct redfish_client *cli, const char *src, const char *dst)
{
	int csrc_len, cdst_len, ret;
	char csrc[RF_PATH_MAX], cdst[RF_PATH_MAX];
	struct mmm_path_stat_req *m;
	struct mmm_path_stat_req *resp;
	struct mmm_resp *r;
	struct mmm_stat_resp *rstat;
	uint16_t ty;
	size_t m_len;
	struct rf_cli_tls *tls;

	tls = client_get_tls();
	if (IS_ERR(tls)) {
		ret = PTR_ERR(tls);
		goto done;
	}
	csrc_len = canonicalize_path2(csrc, RF_PATH_MAX, src);
	if (csrc_len < 0) {
		ret = csrc_len;
		goto done; 
	}
	cdst_len = canonicalize_path2(cdst, RF_PATH_MAX, dst);
	if (cdst_len < 0) {
		ret = cdst_len;
		goto done; 
	}
	m_len = sizeof(struct mmm_unlink_tree_req) + csrc_len + cdst_len + 1;
	m = calloc_msg(MMM_RENAME_REQ, m_len);
	if (!m) {
		ret = -ENOMEM;
		goto done;
	}
	off = offsetof(struct mmm_unlink_req, data);
	pack_str(m, &off, m_len, csrc);
	pack_str(m, &off, m_len, cdst);
	resp = fishc_do_mds_rpc(cli, tls, m);
	if (IS_ERR(resp)) {
		ret = PTR_ERR(resp);
		goto done_release_m;
	}
	r = MSG_DYNAMIC_CAST(resp, MMM_RESP, sizeof(struct mmm_resp));
	if (!r) {
		ret = -EIO;
		goto done_release_resp;
	}
	ret = unpack_from_be32(&r->error);

done_release_resp:
	msg_release(resp);
done_release_m:
	msg_release(m);
done:
	return FORCE_NEGATIVE(ret);
}

int redfish_close(struct redfish_file *ofe)
{
	int fret;

	/* Close = hflush + shutdown */
	pthread_mutex_lock(&ofe->lock);
	if (ofe->flags & RF_FILE_FLAG_SHUTDOWN) {
		/* We can't close an already-closed file */
		pthread_mutex_unlock(&ofe->lock);
		return -EBADF;
	}
	fret = redfish_hflush_impl(ofe);
	pthread_mutex_lock(&ofe->lock);
	redfish_file_shutdown(ofe);
	pthread_mutex_unlock(&ofe->lock);
	return fret;
}

void redfish_free_file(struct redfish_file *ofe)
{
	if (!(ofe->flags & RF_FILE_FLAG_SHUTDOWN)) {
		/* Normally, the client should always close before freeing the
		 * file.  However, sometimes either the programmer forgets, or
		 * the client needs to exit in a hurry. */
		CLIENT_LOG(cli, "redfish_free_file(%s): freeing file "
			"without properly closing!", ofe->path);
		if (ofe->flags & RF_FILE_FLAG_WRITABLE) {
			/* Don't whine about data loss unless the file is open
			 * for write. */
			CLIENT_LOG(cli, "Data may be lost.\n");
		}
		else {
			CLIENT_LOG(cli, "\n");
		}
		redfish_file_shutdown(ofe);
	}
	pthread_mutex_destroy(&ofe->lock);
	/* Release the client we were referencing, so that its memory can now be
	 * freed if necessary. */
	redfish_release_client(ofe->cli);
}
