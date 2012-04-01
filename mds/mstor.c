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

#include "common/config/mstorc.h"
#include "core/glitch_log.h"
#include "jorm/jorm_const.h"
#include "mds/const.h"
#include "mds/mstor.h"
#include "mds/srange_lock.h"
#include "mds/user.h"
#include "msg/types.h"
#include "msg/xdr.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/macro.h"
#include "util/packed.h"
#include "util/path.h"
#include "util/simple_io.h"
#include "util/string.h"

#include <errno.h>
#include <inttypes.h>
#include <leveldb/c.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* leveldb storage scheme:
 * for file and directory nodes:
 *	n[8-byte node-id] => mnode
 * for files:
 *      f[8-byte node-id][8-byte offset] => 8-byte chunk ID
 * for directory children:
 *	c[8-byte node-id][child-name] => 8-byte child ID
 * for chunks:
 *      h[8-byte-chunk-id] => <packed-array of 4-byte OSD-IDs>
 * for zombie chunks:
 *      z[8-byte-death-time][8-byte-zombie-chunk-id] => []
 */
/****************************** constants ********************************/

#define MSTOR_CUR_VERSION 0x000000001U
#define MSTOR_VERSION_MAGIC "Fish"
#define MSTOR_VERSION_MAGIC_LEN 4
#define MSTOR_VERSION_BODY_LEN 8
#define MSTOR_VERSION_INVAL 0xffffffffU

/** TODO: allocate chunk IDs in a per-delegation fashion */
#define MSTOR_INIT_CID 1

#define MSTOR_ROOT_NID 0

#define MSTOR_PERM_EXEC 01
#define MSTOR_PERM_WRITE 02
#define MSTOR_PERM_READ 04

#define MSTOR_NID_MAX 0xffffffffffff0000ULL
#define MSTOR_CID_MAX 0xffffffffffff0000ULL
#define MNODE_KEY_LEN (1 + sizeof(uint64_t))
#define MCHILD_KEY_LEN_PREFIX (1 + sizeof(uint64_t))
#define MCHUNK_KEY_LEN (1 + sizeof(uint64_t))
#define MFILE_KEY_LEN (1 + sizeof(uint64_t) + sizeof(uint64_t))
#define MCHILD_KEY_MAX (1 + sizeof(uint64_t) + RF_PCOMP_MAX)
#define MZOMBIE_KEY_LEN (1 + sizeof(uint64_t) + sizeof(uint64_t))

#define MREQ_FLAG_CHECK_PERMS 0x1
#define TMP_CINFO_BUF_SZ 64

/****************************** prototypes ********************************/
struct mnode;
static void mstor_leveldb_shutdown(struct mstor *mstor);
static int fill_rf_stat(struct mstor *mstor, struct rf_stat *stat,
		const struct mnode *cnode);
static int fill_rf_lentry(struct mstor *mstor, struct rf_lentry *le,
		struct mnode *node, const char *path);

/****************************** types ********************************/
/** A metadata node representing either a file or a directory
 */
struct mnode {
	/** Node id */
	uint64_t nid;
	/** Pointer to data record */
	struct mnode_payload *val;
};

// TODO: create separate mfile / mdir structures.  mdir doesn't need length and
// atime.
PACKED(
struct mnode_payload {
	uint64_t mtime;
	uint64_t atime;
	uint64_t length;
	uint32_t uid;
	uint32_t gid;
	uint16_t mode_and_type;
});

struct mstor {
	/** leveldb database */
	leveldb_t *ldb;
	/** leveldb read options */
	leveldb_readoptions_t *lreadopt;
	/** leveldb write options */
	leveldb_writeoptions_t *lwropt;
	/** leveldb LRU cache */
	leveldb_cache_t *lcache;
	/** Next node ID to use */
	uint64_t next_nid;
	/** The minimum number of seconds that we will sequester a file before
	 * deleting it. */
	int min_zombie_time;
	/** Minimum replication level */
	int min_repl;
	/** Mandated replication level */
	int man_repl;
	/** Protects next_nid */
	pthread_mutex_t next_nid_lock;
	/** Next node ID to use */
	uint64_t next_cid;
	/** Protects next_cid */
	pthread_mutex_t next_cid_lock;
	/** user data.  You cannot modify this without quiescing all threads
	 * that modify the mstor. */
	struct udata *udata;
	/** Tracker for string range locks */
	struct srange_tracker *tk;
};

/****************************** functions ********************************/
static int mstor_range_lock_by_op(struct mstor *mstor, struct mreq *mreq)
{
	struct srange_locker *lk = mreq->lk;

	switch (mreq->op) {
	case MSTOR_OP_CREAT:
	case MSTOR_OP_OPEN:
	case MSTOR_OP_UNLINK:
	case MSTOR_OP_LISTDIR:
	case MSTOR_OP_STAT:
		/* Lock /a/b/ to /a/b/ */
		do_dirname(mreq->full_path, (char*)lk->range[0].start, RF_PATH_MAX);
		canon_path_add_suffix((char*)lk->range[0].start,
				RF_PATH_MAX + 1, '/');
		snprintf((char*)lk->range[0].end, RF_PATH_MAX + 1,
			"%s", (char*)lk->range[0].start);
		/* Lock /a/b/c/ to /a/b/c/ */
		snprintf((char*)lk->range[1].start, RF_PATH_MAX,
			"%s", mreq->full_path);
		canon_path_add_suffix((char*)lk->range[1].start,
				RF_PATH_MAX + 1, '/');
		snprintf((char*)lk->range[1].end, RF_PATH_MAX + 1,
			"%s", (char*)lk->range[1].start);
		mreq->lk->num_range = 2;
		srange_lock(mstor->tk, mreq->lk);
		return 1;
	case MSTOR_OP_RMDIR:
	case MSTOR_OP_MKDIRS:
		/* Note: we might be able to do better than than this for
		 * mkdirs, with a little bit of cleverness */
		/* Lock /a/b/ to /a/b/ */
		do_dirname(mreq->full_path, (char*)lk->range[0].start,
				RF_PATH_MAX);
		canon_path_add_suffix((char*)lk->range[0].start,
				RF_PATH_MAX + 1, '/');
		snprintf((char*)lk->range[0].end, RF_PATH_MAX,
			"%s", (char*)lk->range[0].start);
		/* Lock /a/b/c/ to /a/b/c0 */
		snprintf((char*)lk->range[1].start, RF_PATH_MAX,
			"%s", mreq->full_path);
		canon_path_add_suffix((char*)lk->range[1].start,
				RF_PATH_MAX + 1, '/');
		snprintf((char*)lk->range[1].end, RF_PATH_MAX,
			"%s", mreq->full_path);
		canon_path_add_suffix((char*)lk->range[1].end,
				RF_PATH_MAX + 1, '0');
		mreq->lk->num_range = 2;
		srange_lock(mstor->tk, mreq->lk);
		return 1;
	case MSTOR_OP_CHUNKFIND:
	case MSTOR_OP_CHMOD:
	case MSTOR_OP_CHOWN:
	case MSTOR_OP_UTIMES:
		/* Lock /a/b/ to /a/b/ */
		do_dirname(mreq->full_path, (char*)lk->range[0].start, RF_PATH_MAX);
		canon_path_add_suffix((char*)lk->range[0].start,
				RF_PATH_MAX + 1, '/');
		snprintf((char*)lk->range[0].end, RF_PATH_MAX + 1,
			"%s", (char*)lk->range[0].start);
		mreq->lk->num_range = 1;
		srange_lock(mstor->tk, mreq->lk);
		return 1;
	case MSTOR_OP_RENAME:
		/* Note: this could be made finer-grained by taking 4 locks
		 * instead of 2... */
		/* Assuming we're moving /a/b/c to /d/e/f */
		/* Lock /a/b/ to /a/b0 */
		do_dirname(mreq->full_path, (char*)lk->range[0].start, RF_PATH_MAX);
		snprintf((char*)lk->range[0].end, RF_PATH_MAX,
			"%s", (char*)lk->range[0].start);
		canon_path_add_suffix((char*)lk->range[0].start,
				RF_PATH_MAX + 1, '/');
		canon_path_add_suffix((char*)lk->range[0].end,
				RF_PATH_MAX + 1, '0');
		/* Lock /d/e/f/ to /d/e/f/ */
		do_dirname(((struct mreq_rename*)mreq)->dst_path,
			   (char*)lk->range[1].start, RF_PATH_MAX);
		canon_path_add_suffix((char*)lk->range[1].start,
				RF_PATH_MAX + 1, '/');
		snprintf((char*)lk->range[1].end, RF_PATH_MAX + 1,
			"%s", (char*)lk->range[1].start);
		mreq->lk->num_range = 2;
		srange_lock(mstor->tk, mreq->lk);
		return 1;
	case MSTOR_OP_CHUNKALLOC:
	case MSTOR_OP_FIND_ZOMBIES:
	case MSTOR_OP_DESTROY_ZOMBIE:
		/* These operations don't take a range lock */
		return 0;
	case MSTOR_OP_NODE_SEARCH:
	default:
		/** Logic error */
		abort();
		return 0;
	}
}

BUILD_BUG_ON(SRANGE_LOCKER_MAX_RANGE < 2);

const char *mstor_op_ty_to_str(enum mstor_op_ty op)
{
	switch (op) {
	case MSTOR_OP_CREAT:
		return "MSTOR_OP_CREAT";
	case MSTOR_OP_OPEN:
		return "MSTOR_OP_OPEN";
	case MSTOR_OP_CHUNKFIND:
		return "MSTOR_OP_CHUNKFIND";
	case MSTOR_OP_CHUNKALLOC:
		return "MSTOR_OP_CHUNKALLOC";
	case MSTOR_OP_MKDIRS:
		return "MSTOR_OP_MKDIRS";
	case MSTOR_OP_LISTDIR:
		return "MSTOR_OP_LISTDIR";
	case MSTOR_OP_STAT:
		return "MSTOR_OP_STAT";
	case MSTOR_OP_CHMOD:
		return "MSTOR_OP_CHMOD";
	case MSTOR_OP_CHOWN:
		return "MSTOR_OP_CHOWN";
	case MSTOR_OP_UTIMES:
		return "MSTOR_OP_UTIMES";
	case MSTOR_OP_RMDIR:
		return "MSTOR_OP_RMDIR";
	case MSTOR_OP_UNLINK:
		return "MSTOR_OP_UNLINK";
	case MSTOR_OP_FIND_ZOMBIES:
		return "MSTOR_OP_FIND_ZOMBIES";
	case MSTOR_OP_DESTROY_ZOMBIE:
		return "MSTOR_OP_DESTROY_ZOMBIE";
	case MSTOR_OP_RENAME:
		return "MSTOR_OP_RENAME";
	case MSTOR_OP_NODE_SEARCH:
		return "MSTOR_OP_NODE_SEARCH";
	default:
		break;
	}
	return "(unknown)";
}

/** Get the next available node ID
 *
 * This could be improved to use thread-local storage to cache some available
 * nids on a per-thread basis.
 *
 * Node allocation (and finding highest node, etc) also needs to change to
 * partition the node ids by MDS.  This is probably a simple matter of stealing
 * the highest byte of the ID as an MDS ID.
 *
 * TODO: delegation-local free NID pools
 */
static uint64_t mstor_next_nid(struct mstor *mstor)
{
	uint64_t nid;

#if __WORDSIZE == 32
	pthread_mutex_lock(&mstor->next_nid_lock);
	nid = mstor->next_nid++;
	pthread_mutex_unlock(&mstor->next_nid_lock);
#else
	nid = __sync_fetch_and_add(&mstor->next_nid, 1);
#endif
	if (nid > MSTOR_NID_MAX)
		abort();
	return nid;
}

static uint64_t mstor_next_cid(struct mstor *mstor)
{
	uint64_t cid;

#if __WORDSIZE == 32
	pthread_mutex_lock(&mstor->next_cid_lock);
	cid = mstor->next_cid++;
	pthread_mutex_unlock(&mstor->next_cid_lock);
#else
	cid = __sync_fetch_and_add(&mstor->next_cid, 1);
#endif
	if (cid > MSTOR_NID_MAX)
		abort();
	return cid;
}

static void mnode_free(struct mnode *node)
{
	free(node->val);
}

static uint32_t mstor_parse_version(const char *v, size_t vlen)
{
	uint32_t vers;

	if (vlen != MSTOR_VERSION_BODY_LEN) {
		glitch_log("mstor_parse_version: unknown version length "
			   "%Zd\n", vlen);
		return MSTOR_VERSION_INVAL;
	}
	if (memcmp(v, MSTOR_VERSION_MAGIC, MSTOR_VERSION_MAGIC_LEN)) {
		glitch_log("mstor_parse_version: bad magic value "
			   "0x%02x%02x%02x%02x\n", v[0], v[1], v[2], v[3]);
		return MSTOR_VERSION_INVAL;
	}
	vers = unpack_from_be32(v + MSTOR_VERSION_MAGIC_LEN);
	return vers;
}

static uint32_t mstor_read_version(struct mstor *mstor)
{
	uint32_t ret;
	char *val = NULL, *err = NULL;
	size_t vlen;

	/* read version entry */
	val = leveldb_get(mstor->ldb, mstor->lreadopt, "v", 1,
			&vlen, &err);
	if (err) {
		glitch_log("mstor_leveldb_setup: error reading "
			"version: '%s'\n", err);
		ret = MSTOR_VERSION_INVAL;
		goto done;
	}
	ret = mstor_parse_version(val, vlen);
done:
	if (val)
		free(val);
	free(err);
	return ret;
}

static int mstor_write_version(struct mstor *mstor, uint32_t vers)
{
	int ret;
	char *err = NULL;
	char val[MSTOR_VERSION_BODY_LEN];

	memcpy(val, MSTOR_VERSION_MAGIC, MSTOR_VERSION_MAGIC_LEN);
	pack_to_be32(val + MSTOR_VERSION_MAGIC_LEN, vers);
	leveldb_put(mstor->ldb, mstor->lwropt, "v", 1, val,
		MSTOR_VERSION_BODY_LEN, &err);
	if (err) {
		glitch_log("mstor_write_version: error writing "
			"version: '%s'\n", err);
		ret = -EIO;
		goto done;
	}
	ret = 0;
done:
	free(err);
	return ret;
}

/** Check if the leveldb database is completely empty.
 *
 * @param mstor		The mstor
 *
 * @return		0 if the leveldb database is not empty
 *			1 if the database is completely empty
 *			negative values on error
 */
static int mstor_leveldb_is_empty(struct mstor *mstor)
{
	int ret;
	leveldb_iterator_t *iter = NULL;

	iter = leveldb_create_iterator(mstor->ldb, mstor->lreadopt);
	if (!iter) {
		ret = -ENOMEM;
		goto done;
	}
	leveldb_iter_seek_to_first(iter);
	ret = !leveldb_iter_valid(iter);

done:
	if (iter)
		leveldb_iter_destroy(iter);
	return ret;
}

/** Create a new db from scratch
 *
 * @param mstor		The mstor
 *
 * @return		0 on success; error code otherwise
 */
static int mstor_leveldb_create_new(struct mstor *mstor)
{
	int ret;
	leveldb_iterator_t *iter = NULL;
	char *err = NULL;
	char nkey[MNODE_KEY_LEN], nbody[sizeof(struct mnode_payload)];
	struct mnode_payload *hdr;
	uint64_t t;

	glitch_log("mstor_leveldb_setup: setting up new mstor\n");
	ret = mstor_write_version(mstor, MSTOR_CUR_VERSION);
	if (ret)
		goto done;
	nkey[0] = 'n';
	pack_to_be64(nkey + 1, MSTOR_ROOT_NID);
	hdr = (struct mnode_payload*)nbody;
	pack_to_be16(&hdr->mode_and_type, MSTOR_ROOT_NID_INIT_MODE | MNODE_IS_DIR);
	t = time(NULL);
	pack_to_be64(&hdr->mtime, t);
	pack_to_be64(&hdr->atime, t);
	pack_to_be64(&hdr->length, 0);
	pack_to_be32(&hdr->uid, RF_SUPERUSER_UID);
	pack_to_be32(&hdr->gid, RF_SUPERUSER_GID);
	leveldb_put(mstor->ldb, mstor->lwropt, nkey, MNODE_KEY_LEN,
			nbody, sizeof(struct mnode_payload), &err);
	if (err) {
		glitch_log("mstor_leveldb_setup: error creating root "
			   "node: '%s'\n", err);
		ret = -EIO;
		goto done;
	}
	mstor->next_nid = MSTOR_ROOT_NID + 1;
	mstor->next_cid = MSTOR_INIT_CID;
	__sync_synchronize();
	ret = 0;

done:
	free(err);
	if (iter)
		leveldb_iter_destroy(iter);
	return ret;
}

/** Find what the next node ID should be by looking for the highest existing
 * node ID.
 *
 * @param iter		A leveldb iterator for our db
 * @param next_nid	(out param) the next node ID to use
 *
 * @return		0 on success; error code otherwise
 */
static int mstor_load_next_nid(leveldb_iterator_t *iter, uint64_t *next_nid)
{
	const char *k;
	size_t klen;
	char nkey[MNODE_KEY_LEN];

	nkey[0] = 'n';
	pack_to_be64(nkey + 1, MSTOR_NID_MAX);
	leveldb_iter_seek(iter, nkey, MNODE_KEY_LEN);
	leveldb_iter_prev(iter);
	if (!leveldb_iter_valid(iter)) {
		glitch_log("mstor_load_highest_nid: failed to seek to highest "
			   "node id.\n");
		return -EINVAL;
	}
	k = leveldb_iter_key(iter, &klen);
	if ((klen != MNODE_KEY_LEN) || (k[0] != 'n')) {
		glitch_log("mstor_leveldb_setup: failed to find "
			"highest node ID in use\n");
		return -EINVAL;
	}
	*next_nid = unpack_from_be64(k + 1) + 1;
	return 0;
}

/** Find what the next chunk ID should be by looking for the highest existing
 * chunk ID.
 *
 * @param iter		A leveldb iterator for our db
 * @param next_cid	(out param) the next chunk ID to use
 *
 * @return		0 on success; error code otherwise
 */
static int mstor_load_next_cid(leveldb_iterator_t *iter, uint64_t *next_cid)
{
	const char *k;
	size_t klen;
	char hkey[MCHUNK_KEY_LEN];

	hkey[0] = 'h';
	pack_to_be64(hkey + 1, MSTOR_CID_MAX);
	leveldb_iter_seek(iter, hkey, MCHUNK_KEY_LEN);
	leveldb_iter_prev(iter);
	if (!leveldb_iter_valid(iter)) {
		*next_cid = MSTOR_INIT_CID;
		return 0;
	}
	k = leveldb_iter_key(iter, &klen);
	if ((klen != MCHUNK_KEY_LEN) || (k[0] != 'h')) {
		*next_cid = MSTOR_INIT_CID;
		return 0;
	}
	*next_cid = unpack_from_be64(k + 1) + 1;
	return 0;
}

static int mstor_leveldb_load(struct mstor *mstor)
{
	int ret;
	leveldb_iterator_t *iter = NULL;
	uint32_t vers;

	vers = mstor_read_version(mstor);
	if (vers == MSTOR_VERSION_INVAL) {
		glitch_log("mstor_leveldb_load: failed to read version "
			   "information.\n");
		ret = -EINVAL;
		goto done;
	}
	if (vers != MSTOR_CUR_VERSION) {
		glitch_log("mstor_leveldb_load: can't understand version "
			   "%d of the mstor format.\n", vers);
		ret = -EINVAL;
		goto done;
	}
	iter = leveldb_create_iterator(mstor->ldb, mstor->lreadopt);
	if (!iter) {
		ret = -ENOMEM;
		goto done;
	}
	ret = mstor_load_next_nid(iter, &mstor->next_nid);
	if (ret)
		goto done;
	ret = mstor_load_next_cid(iter, &mstor->next_cid);
	if (ret)
		goto done;
	__sync_synchronize();
	glitch_log("mstor_leveldb_setup: using existing mstor.  "
		"next_nid = 0x%"PRIx64", next_cid = 0x%"PRIx64"\n",
		mstor->next_nid, mstor->next_cid);
	ret = 0;

done:
	if (iter)
		leveldb_iter_destroy(iter);
	return ret;
}

static int mstor_perm_check(const struct mnode *node,
		struct mreq *mreq, uint16_t mode, int want)
{
	uint32_t uid, gid;

	if (!(mreq->flags & MREQ_FLAG_CHECK_PERMS)) {
		/* skip permission check */
		return 0;
	}
	/* Check whether everyone has the permission we seek */
	if (want & mode)
		return 0;
	uid = unpack_from_be32(&node->val->uid);
	if (uid == mreq->user->uid) {
		/* Check whether the owner has the permission we seek */
		if ((want << 6) & mode)
			return 0;
	}
	gid = unpack_from_be32(&node->val->gid);
	if (user_in_gid(mreq->user, gid)) {
		/* Check whether the group has the permission we seek */
		if ((want << 3) & mode)
			return 0;
	}
	glitch_log("mstor_perm_check(want=%02o, nid=0x%"PRIx64", "
			"mode=%04o): returning -EPERM\n",
			want, node->nid, mode);
	return -EPERM;
}

static int mstor_mode_check(const struct mnode *node,
			struct mreq *mreq, int want)
{
	uint16_t mode;

	mode = unpack_from_be16(&node->val->mode_and_type);
	if (want & MNODE_IS_DIR) {
		if ((mode & MNODE_IS_DIR) == 0)
			return -ENOTDIR;
	}
	else {
		if (mode & MNODE_IS_DIR)
			return -EISDIR;
	}
	mode &= ~MNODE_IS_DIR;
	want &= ~MNODE_IS_DIR;
	return mstor_perm_check(node, mreq, mode, want);
}

static int mstor_leveldb_init(struct mstor *mstor,
			const struct mstorc *conf)
{
	int ret;
	char *err = NULL;
	leveldb_t *ldb = NULL;
	leveldb_options_t *lopt = NULL;
	leveldb_readoptions_t *lreadopt = NULL;
	leveldb_writeoptions_t *lwropt = NULL;
	leveldb_cache_t *lcache = NULL;
	size_t cache_size;

	lopt = leveldb_options_create();
	if (!lopt) {
		ret = -ENOMEM;
		goto error;
	}
	leveldb_options_set_create_if_missing(lopt, (conf->mstor_create != 0));
	leveldb_options_set_compression(lopt, leveldb_no_compression);
	cache_size = conf->mstor_cache_mb;
	cache_size *= 1024 * 1024;
	lcache = leveldb_cache_create_lru(cache_size);
	leveldb_options_set_cache(lopt, lcache);
	ldb = leveldb_open(lopt, conf->mstor_path, &err);
	leveldb_options_destroy(lopt);
	if (err) {
		ret = -EIO;
		glitch_log("leveldb_open error: %s\n", err);
		goto error;
	}
	lreadopt = leveldb_readoptions_create();
	if (!lreadopt) {
		ret = -ENOMEM;
		goto error;
	}
	lwropt = leveldb_writeoptions_create();
	if (!lwropt) {
		ret = -ENOMEM;
		goto error;
	}
	leveldb_writeoptions_set_sync(lwropt, 1);
	mstor->ldb = ldb;
	mstor->lreadopt = lreadopt;
	mstor->lwropt = lwropt;
	mstor->lcache = lcache;
	mstor->min_zombie_time = conf->min_zombie_time;
	mstor->min_repl = conf->min_repl;
	mstor->man_repl = conf->man_repl;
	return 0;

error:
	free(err);
	if (ldb)
		leveldb_close(ldb);
	if (lreadopt)
		leveldb_readoptions_destroy(lreadopt);
	if (lwropt)
		leveldb_writeoptions_destroy(lwropt);
	if (lcache)
		leveldb_cache_destroy(lcache);
	return ret;
}

struct mstor* mstor_init(POSSIBLY_UNUSED(struct fast_log_mgr *mgr),
		const struct mstorc *conf, struct udata *udata)
{
	int ret;
	struct mstor *mstor;

	mstor = calloc(1, sizeof(struct mstor));
	if (!mstor) {
		ret = ENOMEM;
		goto error;
	}
	mstor->udata = udata;
	ret = pthread_mutex_init(&mstor->next_nid_lock, NULL);
	if (ret)
		goto error_free_mstor;
	mstor->tk = srange_tracker_init(conf->mstor_io_threads);
	if (IS_ERR(mstor->tk))
		goto error_destroy_next_nid_lock;
	ret = pthread_mutex_init(&mstor->next_cid_lock, NULL);
	if (ret)
		goto error_srange_tracker_free;
	ret = mstor_leveldb_init(mstor, conf);
	if (ret)
		goto error_destroy_next_cid_lock;
	ret = mstor_leveldb_is_empty(mstor);
	if (ret < 0)
		goto error_leveldb_shutdown;
	else if (ret == 1) {
		ret = mstor_leveldb_create_new(mstor);
		if (ret)
			goto error_leveldb_shutdown;
	}
	else {
		ret = mstor_leveldb_load(mstor);
		if (ret)
			goto error_leveldb_shutdown;
	}
	return mstor;

error_leveldb_shutdown:
	mstor_leveldb_shutdown(mstor);
error_destroy_next_cid_lock:
	pthread_mutex_destroy(&mstor->next_cid_lock);
error_srange_tracker_free:
	srange_tracker_free(mstor->tk);
error_destroy_next_nid_lock:
	pthread_mutex_destroy(&mstor->next_nid_lock);
error_free_mstor:
	free(mstor);
error:
	glitch_log("mstor_init failed with error %d\n", ret);
	return ERR_PTR(FORCE_POSITIVE(ret));
}

static void mstor_leveldb_shutdown(struct mstor *mstor)
{
	leveldb_readoptions_destroy(mstor->lreadopt);
	leveldb_writeoptions_destroy(mstor->lwropt);
	leveldb_cache_destroy(mstor->lcache);
	leveldb_close(mstor->ldb);
}

void mstor_shutdown(struct mstor *mstor)
{
	glitch_log("mstor_shutdown: shutting down mstor\n");
	mstor_leveldb_shutdown(mstor);
	pthread_mutex_destroy(&mstor->next_nid_lock);
	pthread_mutex_destroy(&mstor->next_cid_lock);
	srange_tracker_free(mstor->tk);
	free(mstor);
}

static int mstor_fetch_node(struct mstor *mstor, uint64_t nid,
			struct mnode *node)
{
	char *val, *err = NULL;
	size_t vlen;
	char nkey[MNODE_KEY_LEN];

	nkey[0] = 'n';
	pack_to_be64(nkey + 1, nid);
	val = leveldb_get(mstor->ldb, mstor->lreadopt, nkey, MNODE_KEY_LEN,
				&vlen, &err);
	if (err) {
		glitch_log("mstor_fetch_node: leveldb_get(%" PRIx64 ") "
			   "returned error '%s'\n", nid, err);
		free(err);
		return -EIO;
	}
	if (!val) {
		return -ENOENT;
	}
	if (vlen != sizeof(struct mnode_payload)) {
		glitch_log("mstor_fetch_node: unexpected payload size: "
			"got %Zd, expected %Zd\n",
			vlen, sizeof(struct mnode_payload));
		free(err);
		free(val);
		return -EIO;
	}
	node->nid = nid;
	node->val =  (struct mnode_payload *)val;
	return 0;
}

static int mstor_fetch_child(struct mstor *mstor, struct mreq *mreq,
	const char *pcomp, const struct mnode *pnode, struct mnode *cnode)
{
	int ret;
	char ckey[MCHILD_KEY_MAX];
	char *val, *err = NULL;
	size_t klen, vlen;
	uint64_t cnid;

	/* Do we have the permission to look up this child? */
	ret = mstor_mode_check(pnode, mreq,
			MSTOR_PERM_EXEC | MNODE_IS_DIR);
	if (ret)
		return ret;
	/* Look up the child nid */
	ckey[0] = 'c';
	pack_to_be64(ckey + 1, pnode->nid);
	snprintf(ckey + 1 + sizeof(uint64_t), RF_PCOMP_MAX,
		"%s", pcomp);
	klen = 1 + sizeof(uint64_t) + strlen(pcomp);
	val = leveldb_get(mstor->ldb, mstor->lreadopt, ckey,
			klen, &vlen, &err);
	if (err) {
		glitch_log("leveldb_get(0x%" PRIx64 ", %s) returned error "
			   "'%s'\n", pnode->nid, pcomp, err);
		free(err);
		return -EIO;
	}
	if (!val) {
		/* not found */
		return -ENOENT;
	}
	if (vlen != sizeof(uint64_t)) {
		glitch_log("leveldb_get(0x%" PRIx64 ", %s) returned malformed "
			   "val of length %Zd\n", pnode->nid, pcomp, vlen);
		free(val);
		return -EIO;
	}
	cnid = unpack_from_be64(val);
	free(val);
	/* Look up the child node */
	ret = mstor_fetch_node(mstor, cnid, cnode);
	return ret;
}

static int mstor_make_node(struct mstor *mstor, uint16_t mode_and_type,
	uint64_t mtime, uint64_t atime, uint32_t uid, uint32_t gid,
	const char *pcomp, const struct mnode *pnode, struct mnode *cnode)
{
	int ret;
	uint64_t cnid;
	leveldb_writebatch_t* bat = NULL;
	char ckey[MCHILD_KEY_MAX], nkey[MNODE_KEY_LEN];
	char *body = NULL, *err = NULL;
	size_t plen;
	struct mnode_payload *hdr;

	cnid = mstor_next_nid(mstor);
	if (cnid == MSTOR_NID_MAX) {
		ret =-EOVERFLOW;
		goto error;
	}
	body = calloc(1, sizeof(struct mnode_payload));
	if (!body) {
		ret = -ENOMEM;
		goto error;
	}
	bat = leveldb_writebatch_create();
	if (!bat) {
		ret = -ENOMEM;
		goto error;
	}
	ckey[0] = 'c';
	pack_to_be64(ckey + 1, pnode->nid);
	snprintf(ckey + 1 + sizeof(uint64_t), RF_PCOMP_MAX,
		"%s", pcomp);
	plen = 1 + sizeof(uint64_t) + strlen(pcomp);
	nkey[0] = 'n';
	pack_to_be64(nkey + 1, cnid);
	hdr = (struct mnode_payload*)body;
	pack_to_be16(&hdr->mode_and_type, mode_and_type);
	pack_to_be64(&hdr->mtime, mtime);
	pack_to_be64(&hdr->atime, atime);
	pack_to_be32(&hdr->uid, uid);
	pack_to_be32(&hdr->gid, gid);
	leveldb_writebatch_put(bat, ckey, plen, nkey + 1, sizeof(uint64_t));
	leveldb_writebatch_put(bat, nkey, MNODE_KEY_LEN, body,
			sizeof(struct mnode_payload));
	leveldb_write(mstor->ldb, mstor->lwropt, bat, &err);
	if (err) {
		glitch_log("leveldb_write(%" PRIx64 ") returned error '%s'\n",
			cnid, err);
		ret = -EIO;
		goto error;
	}
	cnode->nid = cnid;
	cnode->val = (struct mnode_payload*)body;
	leveldb_writebatch_destroy(bat);
	return 0;

error:
	if (bat)
		leveldb_writebatch_destroy(bat);
	free(body);
	free(err);
	return ret;
}

static int mstor_do_creat(struct mstor *mstor, struct mreq *mreq,
		const char *pcomp, const struct mnode *pnode,
		struct mnode *cnode)
{
	int ret;
	struct mreq_creat *req;

	ret = mstor_mode_check(pnode, mreq, MSTOR_PERM_WRITE | MNODE_IS_DIR);
	if (ret)
		return ret;
	req = (struct mreq_creat*)mreq;
	ret = mstor_make_node(mstor, req->mode, req->ctime, req->ctime,
		mreq->user->uid, mreq->user->gid, pcomp, pnode, cnode);
	if (ret == 0)
		req->nid = cnode->nid;
	return ret;
}

static int mstor_do_open(struct mstor *mstor, struct mreq *mreq,
		struct mnode *node)
{
	int ret;
	char k[MNODE_KEY_LEN], *err = NULL;
	struct mnode_payload *hdr;
	struct mreq_open *req;

	/* Do we have permission to open this file?  And is it a file, rather
	 * than a directory? */
	ret = mstor_mode_check(node, mreq, MSTOR_PERM_READ);
	if (ret)
		return ret;
	/* Update atime */
	req = (struct mreq_open *)mreq;
	hdr = (struct mnode_payload*)node->val;
	pack_to_be64(&hdr->atime, req->atime);
	k[0] = 'n';
	pack_to_be64(k + 1, node->nid);
	leveldb_put(mstor->ldb, mstor->lwropt, k, MNODE_KEY_LEN,
		(const char*)node->val, sizeof(struct mnode_payload), &err);
	if (err) {
		glitch_log("mstor_do_open(nid=0x%"PRIx64": leveldb_put "
			"returned error '%s'\n", node->nid, err);
		ret = -EIO;
		goto done;
	}
	req->nid = node->nid;
	ret = 0;

done:
	free(err);
	return ret;
}

static int mstor_chunkfind_impl(struct mstor *mstor, uint64_t nid,
		struct chunk_info *cinfos, int max_cinfos,
		uint64_t start, uint64_t end)
{
	int ret, num_cinfos = 0;
	char fkey[MFILE_KEY_LEN];
	leveldb_iterator_t *iter = NULL;
	const char *k;
	const char *v;
	size_t klen, vlen;
	uint64_t base;

	/* Get starting element */
	memset(fkey, 0, sizeof(fkey));
	fkey[0] = 'f';
	pack_to_be64(fkey + 1, nid);
	pack_to_be64(fkey + sizeof(uint64_t) + 1, start + 1);
	iter = leveldb_create_iterator(mstor->ldb, mstor->lreadopt);
	if (!iter) {
		glitch_log("mstor_do_chunkfind: leveldb_create_iterator "
			"failed.\n");
		ret = -ENOMEM;
		goto done;
	}
	leveldb_iter_seek(iter, fkey, MFILE_KEY_LEN);
	// TODO: get rid of this leveldb_iter_prev.  The levelDB manual says
	// that reverse iteration may be somewhat slower than forward.
	// We could get rid of this by using a custom comparator or by storing
	// the starting offsets as the negative of their real value (hence
	// getting the 'round down' semantics that we want.)
	leveldb_iter_prev(iter);
	if (!leveldb_iter_valid(iter)) {
		/* no chunk entries found */
		ret = 0;
		goto done;
	}
	k = leveldb_iter_key(iter, &klen);
	if ((klen != MFILE_KEY_LEN) ||
			(memcmp(k, fkey, 1 + sizeof(uint64_t)))) {
		/* no chunk entries found */
		ret = 0;
		goto done;
	}
	base = unpack_from_be64(k + 1 + sizeof(uint64_t));
	v = leveldb_iter_value(iter, &vlen);
	while (1) {
		if (num_cinfos + 1 >= max_cinfos) {
			ret = num_cinfos;
			goto done;
		}
		if (vlen != sizeof(uint64_t)) {
			glitch_log("mstor_do_chunkfind: leveldb_iter_key "
				"got illegal %Zd-length cid\n", vlen);
			ret = -EIO;
			goto done;
		}
		cinfos[num_cinfos].cid = unpack_from_be64(v);
		cinfos[num_cinfos].base = base;
		num_cinfos++;
		leveldb_iter_next(iter);
		if (!leveldb_iter_valid(iter)) {
			break;
		}
		k = leveldb_iter_key(iter, &klen);
		if ((klen != MFILE_KEY_LEN) ||
				(memcmp(k, fkey, 1 + sizeof(uint64_t)))) {
			break;
		}
		base = unpack_from_be64(k + 1 + sizeof(uint64_t));
		if (base > end)
			break;
		v = leveldb_iter_value(iter, &vlen);
	}
	ret = num_cinfos;
done:
	if (iter)
		leveldb_iter_destroy(iter);
	return ret;
}

static int mstor_do_chunkfind(struct mstor *mstor, struct mreq *mreq,
		const struct mnode *cnode)
{
	int ret;
	struct mreq_chunkfind *req;

	/** TODO: have to return OSD IDs here too! */
	ret = mstor_mode_check(cnode, mreq, MSTOR_PERM_READ);
	if (ret)
		return ret;
	req = (struct mreq_chunkfind*)mreq;
	ret = mstor_chunkfind_impl(mstor, cnode->nid,
			req->cinfos, req->max_cinfos, req->start, req->end);
	if (ret < 0)
		return ret;
	req->num_cinfos = ret;
	return 0;
}

static int mstor_assign_oid(POSSIBLY_UNUSED(struct mstor *mstor),
		uint32_t *oid)
{
	/** TODO: actually implement OSD ID assignment */
	oid[0] = 123;
	oid[1] = 456;
	return 2;
}

static int mstor_do_chunkalloc(struct mstor *mstor, struct mreq *mreq)
{
	int ret, num_oid;
	char fkey[MFILE_KEY_LEN], hkey[MCHUNK_KEY_LEN], *err = NULL;
	struct mreq_chunkalloc *req;
	struct mnode node;
	struct chunk_info cinfo;
	uint64_t cid, be_cid;
	uint32_t oids[RF_MAX_REPLICAS];
	leveldb_writebatch_t* bat = NULL;

	memset(&node, 0, sizeof(node));
	req = (struct mreq_chunkalloc*)mreq;
	ret = mstor_fetch_node(mstor, req->nid, &node);
	if (ret)
		goto done;
	ret = mstor_mode_check(&node, mreq, MSTOR_PERM_WRITE);
	if (ret)
		goto done;
	ret = mstor_chunkfind_impl(mstor, req->nid, &cinfo, 1,
			req->off, req->off);
	if (ret < 0)
		goto done;
	else if (ret != 0) {
		/* Tried to allocate a new chunk that came before some other
		 * chunks */
		ret = -EINVAL;
		goto done;
	}
	bat = leveldb_writebatch_create();
	if (!bat) {
		ret = -ENOMEM;
		goto done;
	}
	/** TODO: update mtime here? */
	memset(fkey, 0, sizeof(fkey));
	fkey[0] = 'f';
	pack_to_be64(fkey + 1, req->nid);
	pack_to_be64(fkey + sizeof(uint64_t) + 1, req->off);
	cid = mstor_next_cid(mstor);
	num_oid = mstor_assign_oid(mstor, oids);
	if (num_oid < 0) {
		ret = num_oid;
		goto done;
	}
	pack_to_be64(&be_cid, cid);
	leveldb_writebatch_put(bat, fkey, MFILE_KEY_LEN,
			(const char*)&be_cid, sizeof(be_cid));
	hkey[0] = 'h';
	pack_to_be64(hkey + 1, cid);
	leveldb_writebatch_put(bat, hkey, MCHUNK_KEY_LEN,
			(const char *)oids, sizeof(uint32_t) * num_oid);
	leveldb_write(mstor->ldb, mstor->lwropt, bat, &err);
	if (err) {
		glitch_log("mstor_do_chunkalloc(%" PRIx64 "): leveldb_write "
			"returned error '%s'\n", req->nid, err);
		ret = -EIO;
		goto done;
	}
	req->cid = cid;
	memcpy(req->oid, oids, sizeof(req->oid));
	req->num_oid = num_oid;

done:
	if (bat)
		leveldb_writebatch_destroy(bat);
	free(err);
	mnode_free(&node);
	return ret;
}

static int mstor_do_mkdir(struct mstor *mstor, struct mreq *mreq,
		const char *pcomp, const struct mnode *pnode,
		struct mnode *cnode)
{
	int ret;
	struct mreq_mkdirs *req;

	ret = mstor_mode_check(pnode, mreq, MSTOR_PERM_WRITE | MNODE_IS_DIR);
	if (ret)
		return ret;
	req = (struct mreq_mkdirs*)mreq;
	ret = mstor_make_node(mstor, req->mode | MNODE_IS_DIR,
		req->ctime, req->ctime, mreq->user->uid,
		mreq->user->gid, pcomp, pnode, cnode);
	return ret;
}

static int mstor_do_listdir(struct mstor *mstor, struct mreq *mreq,
		const struct mnode *dnode)
{
	int i, ret, num_stat = 0;
	char *err = NULL;
	leveldb_iterator_t *iter = NULL;
	const char *k;
	const char *v;
	char ckey[1 + sizeof(uint64_t)], pcomp[RF_PCOMP_MAX];
	size_t klen, vlen;
	struct mnode node;
	uint64_t nid;
	struct mreq_listdir *req;

	req = (struct mreq_listdir*)mreq;
	memset(&node, 0, sizeof(struct mnode));
	ret = mstor_mode_check(dnode, mreq,
			MSTOR_PERM_READ | MNODE_IS_DIR);
	if (ret)
		goto done;
	iter = leveldb_create_iterator(mstor->ldb, mstor->lreadopt);
	if (!iter) {
		glitch_log("mstor_do_listdir: leveldb_create_iterator failed.\n");
		ret = -ENOMEM;
		goto done;
	}
	ckey[0] = 'c';
	pack_to_be64(ckey + 1, dnode->nid);
	leveldb_iter_seek(iter, ckey, sizeof(ckey));
	while (1) {
		if (!leveldb_iter_valid(iter)) {
			break;
		}
		k = leveldb_iter_key(iter, &klen);
		v = leveldb_iter_value(iter, &vlen);
		if (klen < 1) {
			glitch_log("mstor_do_listdir: leveldb_iter_key "
				"got illegal 0-length key\n");
			ret = -EIO;
			goto done;
		}
		if (k[0] != 'c')
			break;
		if (klen <= MCHILD_KEY_LEN_PREFIX) {
			glitch_log("mstor_do_listdir: leveldb_iter_key "
				"returned klen = %Zd.  That should not be "
				"possible.\n", klen);
			ret = -EIO;
			goto done;
		}
		nid = unpack_from_be64(k + 1);
		if (nid != dnode->nid)
			break;
		if (vlen != sizeof(uint64_t)) {
			glitch_log("mstor_do_listdir: leveldb_iter_value "
				"returned vlen = %Zd.  That should not be "
				"possible.\n", vlen);
			ret = -EIO;
			goto done;
		}
		if (num_stat >= req->max_stat) {
			ret = -ENAMETOOLONG;
			goto done;
		}
		nid = unpack_from_be64(v);
		if (klen - MCHILD_KEY_LEN_PREFIX >= RF_PCOMP_MAX) {
			ret = -ENAMETOOLONG;
			goto done;
		}
		memcpy(pcomp, k + MCHILD_KEY_LEN_PREFIX,
			klen - MCHILD_KEY_LEN_PREFIX);
		pcomp[klen - MCHILD_KEY_LEN_PREFIX] = '\0';
		ret = mstor_fetch_node(mstor, nid, &node);
		if (ret == -ENOENT) {
			/* possible race between us and a rename/delete */
			goto next;
		}
		else if (ret) {
			/* error condition */
			goto done;
		}
		ret = fill_rf_lentry(mstor, &req->le[num_stat], &node,
			pcomp);
		if (ret)
			goto done;
next:
		mnode_free(&node);
		memset(&node, 0, sizeof(struct mnode));
		leveldb_iter_next(iter);
		++num_stat;
	}
	ret = 0;
done:
	free(err);
	if (iter)
		leveldb_iter_destroy(iter);
	mnode_free(&node);
	if (ret) {
		for (i = 0; i < num_stat; ++i) {
			XDR_REQ_FREE(rf_lentry, &req->le[i]);
		}
		req->num_stat = 0;
	}
	else {
		req->num_stat = num_stat;
	}
	return ret;
}

static int mstor_do_stat(struct mstor *mstor, struct mreq *mreq,
		const struct mnode *pnode,
		const struct mnode *cnode)
{
	int ret;
	struct mreq_stat *req = (struct mreq_stat*)mreq;

	if (pnode->val) {
		/* In order to stat an entry, we need read permissions on its
		 * parent directory.  If pnode->val == NULL, then cnode must be
		 * the root node.  And failing to let users stat the root node
		 * would be quite silly... */
		ret = mstor_mode_check(pnode, mreq,
				MSTOR_PERM_READ | MNODE_IS_DIR);
		if (ret)
			return ret;
	}
	return fill_rf_stat(mstor, &req->stat, cnode);
}

static int fill_rf_stat(struct mstor *mstor, struct rf_stat *stat,
		const struct mnode *node)
{
	struct user *user;
	struct group *group;
	uint64_t uid, gid;

	stat->mtime = unpack_from_be64(&node->val->mtime);
	stat->atime = unpack_from_be64(&node->val->atime);
	stat->length = unpack_from_be64(&node->val->length);
	stat->block_sz = 0; // TODO: fill in
	stat->mode_and_type = unpack_from_be16(&node->val->mode_and_type);
	// TODO: support custom per-file replication settings
	stat->man_repl = mstor->man_repl;
	uid = unpack_from_be32(&node->val->uid);
	user = udata_lookup_uid(mstor->udata, uid);
	stat->user = strdup(user->name);
	if (!stat->user)
		return -ENOMEM;
	gid = unpack_from_be32(&node->val->gid);
	group = udata_lookup_gid(mstor->udata, gid);
	stat->group = strdup(group->name);
	if (!stat->group) {
		free(stat->user);
		return -ENOMEM;
	}
	return 0;
}

static int fill_rf_lentry(struct mstor *mstor, struct rf_lentry *le,
		struct mnode *node, const char *pcomp)
{
	int ret;

	le->pcomp = strdup(pcomp);
	if (!le->pcomp)
		return -ENAMETOOLONG;
	ret = fill_rf_stat(mstor, &le->stat, node);
	if (ret) {
		free(le->pcomp);
		return ret;
	}
	return 0;
}

static int mstor_do_chmod(struct mstor *mstor, struct mreq *mreq,
		const struct mnode *node)
{
	int ret;
	struct mreq_chmod *req;
	struct mnode_payload *hdr;
	char nkey[MNODE_KEY_LEN], *err = NULL;
	uint16_t old_mode_and_type, mode_and_type;

	req = (struct mreq_chmod*)mreq;
	hdr = (struct mnode_payload*)node->val;
	mode_and_type = req->mode;
	old_mode_and_type = unpack_from_be16(&hdr->mode_and_type);
	if (old_mode_and_type & MNODE_IS_DIR)
		mode_and_type |= MNODE_IS_DIR;
	else
		mode_and_type &= ~MNODE_IS_DIR;
	pack_to_be16(&hdr->mode_and_type, mode_and_type);
	nkey[0] = 'n';
	pack_to_be64(nkey + 1, node->nid);
	leveldb_put(mstor->ldb, mstor->lwropt, nkey, MNODE_KEY_LEN,
		(const char*)node->val, sizeof(struct mnode_payload), &err);
	if (err) {
		glitch_log("mstor_do_chmod(nid=0x%"PRIx64": leveldb_put "
			"returned error '%s'\n", node->nid, err);
		ret = -EIO;
		goto done;
	}
	ret = 0;

done:
	free(err);
	return ret;
}

static int mstor_do_chown(struct mstor *mstor, struct mreq *mreq,
		const struct mnode *node)
{
	int ret;
	struct mreq_chown *req;
	char nkey[MNODE_KEY_LEN], *err = NULL;
	struct mnode_payload new_node;
	struct user *new_user = NULL;
	struct group *new_group = NULL;

	req = (struct mreq_chown*)mreq;
	memcpy(&new_node, node->val, sizeof(struct mnode_payload));
	if (req->new_user[0]) {
		new_user = udata_lookup_user(mstor->udata, req->new_user);
		if (IS_ERR(new_user))
			return PTR_ERR(new_user);
		pack_to_be32(&new_node.uid, new_user->uid);
	}
	if (req->new_group[0]) {
		new_group = udata_lookup_group(mstor->udata, req->new_group);
		if (IS_ERR(new_group))
			return PTR_ERR(new_group);
		pack_to_be32(&new_node.gid, new_group->gid);
	}
	if (mreq->flags & MREQ_FLAG_CHECK_PERMS) {
		if (new_user) {
			/* Only the superuser can do chown.  And the superuser
			 * will have MREQ_FLAG_CHECK_PERMS cleared. */
			ret = -EPERM;
			goto done;
		}
		else if (new_group) {
			/* Users can do chgrp on things they own, as long as
			 * they are a member of the group they're moving
			 * them into. */
			uint32_t uid = unpack_from_be32(&node->val->uid);

			if ((!user_in_gid(mreq->user, new_group->gid)) ||
					(uid != mreq->user->uid)) {
				ret = -EPERM;
				goto done;
			}
		}
	}
	nkey[0] = 'n';
	pack_to_be64(nkey + 1, node->nid);
	leveldb_put(mstor->ldb, mstor->lwropt, nkey, MNODE_KEY_LEN,
			(const char*)&new_node, sizeof(struct mnode_payload),
			&err);
	if (err) {
		glitch_log("mstor_do_chown(nid=0x%"PRIx64": leveldb_put "
			"returned error '%s'\n", node->nid, err);
		ret = -EIO;
		goto done;
	}
	ret = 0;

done:
	free(err);
	return ret;
}

static int mstor_do_utimes(struct mstor *mstor, struct mreq *mreq,
		const struct mnode *node)
{
	int ret;
	struct mreq_utimes *req;
	struct mnode_payload *hdr;
	char nkey[MNODE_KEY_LEN], *err = NULL;

	req = (struct mreq_utimes*)mreq;
	hdr = (struct mnode_payload*)node->val;
	if (req->new_atime != RF_INVAL_TIME)
		pack_to_be64(&hdr->atime, req->new_atime);
	if (req->new_mtime != RF_INVAL_TIME)
		pack_to_be64(&hdr->mtime, req->new_mtime);
	nkey[0] = 'n';
	pack_to_be64(nkey + 1, node->nid);
	leveldb_put(mstor->ldb, mstor->lwropt, nkey, MNODE_KEY_LEN,
		(const char*)node->val, sizeof(struct mnode_payload), &err);
	if (err) {
		glitch_log("mstor_do_utimes(nid=0x%"PRIx64": leveldb_put "
			"returned error '%s'\n", node->nid, err);
		ret = -EIO;
		goto done;
	}
	ret = 0;

done:
	free(err);
	return ret;
}

static void leveldb_delete_node(const char *pcomp, const struct mnode *pnode,
		const struct mnode *cnode, leveldb_writebatch_t *bat)
{
	char ckey[MCHILD_KEY_MAX], nkey[MNODE_KEY_LEN];

	/* delete directory entry in parent */
	ckey[0] = 'c';
	pack_to_be64(ckey + 1, pnode->nid);
	snprintf(ckey + 1 + sizeof(uint64_t), RF_PCOMP_MAX,
			"%s", pcomp);
	leveldb_writebatch_delete(bat, ckey,
			1 + sizeof(uint64_t) + strlen(pcomp));
	/* delete node entry */
	nkey[0] = 'n';
	pack_to_be64(nkey + 1, cnode->nid);
	leveldb_writebatch_delete(bat, nkey, MNODE_KEY_LEN);
}

static int leveldb_delete_chunks(struct mstor *mstor,
		const struct mnode *cnode, leveldb_writebatch_t *bat,
		uint64_t ztime)
{
	int i, ret, ninfo;
	uint64_t start = 0;
	struct chunk_info cinfos[TMP_CINFO_BUF_SZ];
	char fkey[MFILE_KEY_LEN], zkey[MZOMBIE_KEY_LEN];

	while (1) {
		ret = mstor_chunkfind_impl(mstor, cnode->nid, cinfos,
			TMP_CINFO_BUF_SZ, start, 0xffffffffffffffffLLU);
		if (ret < 0)
			return ret;
		ninfo = ret;
		fkey[0] = 'f';
		pack_to_be64(fkey + 1, cnode->nid);
		zkey[0] = 'z';
		pack_to_be64(zkey + 1, ztime);
		for (i = 0; i < ninfo; ++i) {
			/* remove file chunk entry, add zombie chunk table
			 * entry */
			pack_to_be64(fkey + sizeof(uint64_t) + 1,
				cinfos[i].base);
			leveldb_writebatch_delete(bat, fkey, MFILE_KEY_LEN);
			pack_to_be64(zkey + sizeof(uint64_t) + 1,
				cinfos[i].cid);
			leveldb_writebatch_put(bat, zkey, MZOMBIE_KEY_LEN,
				NULL, 0);
		}
		if (ninfo + 1 < TMP_CINFO_BUF_SZ)
			break;
		start = cinfos[ninfo - 1].base + 1;
	}
	return 0;
}

static int mstor_do_rmdir(struct mstor *mstor, struct mreq *mreq,
		const char* pcomp, const struct mnode *pnode,
		const struct mnode *cnode)
{
	int ret;
	char *err = NULL;
	leveldb_iterator_t *iter = NULL;
	leveldb_writebatch_t *bat = NULL;
	const char *k;
	const char *v;
	char ckey[1 + sizeof(uint64_t)], pcomp2[RF_PCOMP_MAX];
	size_t klen, vlen;
	struct mnode node;
	uint64_t nid, ztime;
	struct mreq_rmdir *req;
	uint16_t mode_and_type;

	req = (struct mreq_rmdir*)mreq;
	ztime = req->ztime;
	memset(&node, 0, sizeof(struct mnode));
	if (pnode->val == NULL) {
		/* You can't delete the root inode. */
		ret = -EINVAL;
		goto done;
	}
	ret = mstor_mode_check(pnode, mreq,
			MSTOR_PERM_WRITE | MNODE_IS_DIR);
	if (ret)
		goto done;
	bat = leveldb_writebatch_create();
	if (!bat) {
		ret = -ENOMEM;
		goto done;
	}
	iter = leveldb_create_iterator(mstor->ldb, mstor->lreadopt);
	if (!iter) {
		ret = -ENOMEM;
		goto done;
	}
	ckey[0] = 'c';
	pack_to_be64(ckey + 1, cnode->nid);
	leveldb_iter_seek(iter, ckey, sizeof(ckey));
	while (1) {
		if (!leveldb_iter_valid(iter))
			break;
		k = leveldb_iter_key(iter, &klen);
		v = leveldb_iter_value(iter, &vlen);
		if (klen < 1) {
			glitch_log("mstor_do_rmdir: leveldb_iter_key "
				"got illegal 0-length key\n");
			ret = -EIO;
			goto done;
		}
		if (k[0] != 'c')
			break;
		if (klen <= MCHILD_KEY_LEN_PREFIX) {
			glitch_log("mstor_do_rmdir: leveldb_iter_key "
				"returned klen = %Zd.  That should not be "
				"possible.\n", klen);
			ret = -EIO;
			goto done;
		}
		nid = unpack_from_be64(k + 1);
		if (nid != cnode->nid)
			break;
		if (vlen != sizeof(uint64_t)) {
			glitch_log("mstor_do_rmdir: leveldb_iter_value "
				"returned vlen = %Zd.  That should not be "
				"possible.\n", vlen);
			ret = -EIO;
			goto done;
		}
		nid = unpack_from_be64(v);
		if (klen - MCHILD_KEY_LEN_PREFIX >= RF_PCOMP_MAX) {
			glitch_log("mstor_do_rmdir: illegally long name "
				"(len = %Zd).\n", klen - MCHILD_KEY_LEN_PREFIX);
			ret = -EIO;
			goto done;
		}
		if (!req->rmr) {
			ret = -ENOTEMPTY;
			goto done;
		}
		memcpy(pcomp2, k + MCHILD_KEY_LEN_PREFIX,
			klen - MCHILD_KEY_LEN_PREFIX);
		pcomp2[klen - MCHILD_KEY_LEN_PREFIX] = '\0';
		ret = mstor_fetch_node(mstor, nid, &node);
		if (ret)
			goto done;
		mode_and_type = unpack_from_be16(&cnode->val->mode_and_type);
		ret = mstor_perm_check(&node, mreq,
			mode_and_type & (~MNODE_IS_DIR), MSTOR_PERM_WRITE);
		if (ret)
			goto done;
		if (!(mode_and_type & MNODE_IS_DIR)) {
			ret = leveldb_delete_chunks(mstor, &node, bat, ztime);
			if (ret)
				goto done;
		}
		leveldb_delete_node(pcomp2, pnode, cnode, bat);
		mnode_free(&node);
		memset(&node, 0, sizeof(struct mnode));
		leveldb_iter_next(iter);
	}
	leveldb_delete_node(pcomp, pnode, cnode, bat);
	/* apply changes */
	leveldb_write(mstor->ldb, mstor->lwropt, bat, &err);
	if (err) {
		glitch_log("mstor_do_rmdir(0x%"PRIx64", %s): "
			"leveldb_write returned error '%s'\n",
			cnode->nid, pcomp, err);
		ret = -EIO;
		goto done;
	}
	ret = 0;
done:
	free(err);
	if (iter)
		leveldb_iter_destroy(iter);
	if (bat)
		leveldb_writebatch_destroy(bat);
	mnode_free(&node);
	return ret;
}

static int mstor_do_unlink(struct mstor *mstor, struct mreq *mreq,
		const char *pcomp, const struct mnode *pnode,
		const struct mnode *cnode)
{
	char *err = NULL;
	struct mreq_unlink *req;
	int ret;
	uint16_t mode_and_type;
	leveldb_writebatch_t *bat = NULL;

	if (pnode->val == NULL) {
		/* You can't delete the root inode. */
		ret = -EISDIR;
		goto done;
	}
	req = (struct mreq_unlink*)mreq;
	mode_and_type = unpack_from_be16(&cnode->val->mode_and_type);
	if (mode_and_type & MNODE_IS_DIR) {
		ret = -EISDIR;
		goto done;
	}
	bat = leveldb_writebatch_create();
	if (!bat) {
		ret = -ENOMEM;
		goto done;
	}
	ret = leveldb_delete_chunks(mstor, cnode, bat, req->ztime);
	if (ret)
		goto done;
	leveldb_delete_node(pcomp, pnode, cnode, bat);
	leveldb_write(mstor->ldb, mstor->lwropt, bat, &err);
	if (err) {
		glitch_log("mstor_do_unlink(0x%"PRIx64", %s): "
			"leveldb_write returned error '%s'\n",
			cnode->nid, pcomp, err);
		ret = -EIO;
		goto done;
	}
done:
	free(err);
	if (bat)
		leveldb_writebatch_destroy(bat);
	return ret;
}

static int mstor_do_find_zombies(struct mstor *mstor, struct mreq *mreq)
{
	int ret, num_res, max_res;
	leveldb_iterator_t *iter = NULL;
	const char *k;
	const char POSSIBLY_UNUSED(*v);
	char zkey[MZOMBIE_KEY_LEN], *err = NULL;
	size_t klen, vlen;
	struct mreq_find_zombies *req;

	req = (struct mreq_find_zombies*)mreq;
	iter = leveldb_create_iterator(mstor->ldb, mstor->lreadopt);
	if (!iter) {
		ret = -ENOMEM;
		goto done;
	}
	zkey[0] = 'z';
	pack_to_be64(zkey + 1, req->lower_bound.ztime);
	pack_to_be64(zkey + sizeof(uint64_t) + 1, req->lower_bound.cid);
	leveldb_iter_seek(iter, zkey, MZOMBIE_KEY_LEN);
	num_res = 0;
	max_res = req->max_res;
	while (1) {
		if (num_res + 1 >= max_res)
			break;
		if (!leveldb_iter_valid(iter))
			break;
		k = leveldb_iter_key(iter, &klen);
		v = leveldb_iter_value(iter, &vlen);
		if (klen < 1) {
			glitch_log("mstor_do_find_zombies: leveldb_iter_key "
				"got illegal 0-length key\n");
			ret = -EIO;
			goto done;
		}
		if (k[0] != 'z')
			break;
		if (klen != MZOMBIE_KEY_LEN) {
			glitch_log("mstor_do_find_zombies: leveldb_iter_key "
				"returned klen = %Zd.  That should not be "
				"possible.\n", klen);
			ret = -EIO;
			goto done;
		}
		if (vlen != 0) {
			glitch_log("mstor_do_find_zombies: leveldb_iter_value "
				"returned vlen = %Zd.  That should not be "
				"possible.\n", vlen);
			ret = -EIO;
			goto done;
		}
		req->zinfos[num_res].ztime = unpack_from_be64(k + 1);
		req->zinfos[num_res].cid =
				unpack_from_be64(k + sizeof(uint64_t) + 1);
		++num_res;
		leveldb_iter_next(iter);
	}
	req->num_res = num_res;
	ret = num_res;
done:
	free(err);
	if (iter)
		leveldb_iter_destroy(iter);
	return ret;
}

static int mstor_do_destroy_zombie(struct mstor *mstor, struct mreq *mreq)
{
	int ret;
	char zkey[MZOMBIE_KEY_LEN], *err = NULL;
	struct mreq_destroy_zombie *req;

	req = (struct mreq_destroy_zombie*)mreq;
	zkey[0] = 'z';
	pack_to_be64(zkey + 1, req->zinfo.ztime);
	pack_to_be64(zkey + sizeof(uint64_t) + 1, req->zinfo.cid);
	leveldb_delete(mstor->ldb, mstor->lwropt, zkey, MZOMBIE_KEY_LEN, &err);
	if (err) {
		glitch_log("mstor_do_destroy_zombie(ztime=0x%"PRIx64", "
			"cid=0x%"PRIx64" got leveldb_delete error '%s'\n",
			req->zinfo.ztime, req->zinfo.cid, err);
		ret = -EIO;
		goto done;
	}
	ret = 0;

done:
	free(err);
	return ret;
}

static int mstor_do_path_operation(struct mstor *mstor, struct mreq *mreq,
			    struct mnode *pnode, struct mnode *cnode)
{
	char *pcomp;
	int ret, npc, cpc;
	char full_path[RF_PATH_MAX];
	uint64_t forbidden;

	mreq->flags = MREQ_FLAG_CHECK_PERMS;
	/* The superuser can do anything */
	if (mreq->user->uid == RF_SUPERUSER_UID)
		mreq->flags &= ~MREQ_FLAG_CHECK_PERMS;
	if (zsnprintf(full_path, sizeof(full_path), "%s", mreq->full_path))
		return -ENAMETOOLONG;
	npc = 0;
	pcomp = full_path;
	while (1) {
		pcomp = index(pcomp, '/');
		if (!pcomp)
			break;
		*pcomp = '\0';
		++pcomp;
		++npc;
	}
	if ((full_path[0] == '\0') && (full_path[1] == '\0')) {
		/* Corner case: normally the number of path components in the
		 * canonicalized path is equal to the number of slashes.
		 * However, the root itself has 0 path components, but is
		 * referred to with a single slash. */
		npc = 0;
	}
	pcomp = full_path;
	cpc = 0;
	ret = mstor_fetch_node(mstor, MSTOR_ROOT_NID, cnode);
	if (ret) {
		glitch_log("mstor_do_operation: couldn't load "
			"root node! Error %d\n", ret);
		return -ENOSYS;
	}
	if (mreq->op == MSTOR_OP_NODE_SEARCH) {
		struct mreq_node_search *req = (struct mreq_node_search*)mreq;
		forbidden = req->forbidden;
	}
	else {
		forbidden = RF_INVAL_NID;
	}
	for (cpc = 0; cpc < npc; ++cpc) {
		mnode_free(pnode);
		memcpy(pnode, cnode, sizeof(struct mnode));
		memset(cnode, 0, sizeof(struct mnode));
		pcomp = memchr(pcomp, '\0', RF_PATH_MAX) + 1;
		if (pnode->nid == forbidden)
			return -EINVAL;
		ret = mstor_fetch_child(mstor, mreq, pcomp, pnode, cnode);
		if (ret == -ENOENT) {
			switch (mreq->op) {
			case MSTOR_OP_CREAT:
				if (cpc < npc - 1)
					return ret;
				return mstor_do_creat(mstor, mreq, pcomp,
						pnode, cnode);
			case MSTOR_OP_MKDIRS:
				ret = mstor_do_mkdir(mstor, mreq, pcomp,
						pnode, cnode);
				if (ret)
					return ret;
				/* When we're creating multiple directories at
				 * once, don't check permissions on the
				 * directories we have just created before
				 * trying to create the nested ones.
				 *
				 * This saves time and makes it possible to
				 * invoke mkdirs with a mode that doesn't
				 * include execute for yourself.
				 * I'm not sure how useful this is, but it seems
				 * surprising and weird for it to always fail,
				 * so we support it.
				 */
				mreq->flags &= ~MREQ_FLAG_CHECK_PERMS;
				break;
			case MSTOR_OP_NODE_SEARCH: {
				struct mreq_node_search *req =
					(struct mreq_node_search*)mreq;
				req->npc_rem = npc - cpc;
				return -ENOENT;
			}
			default:
				return ret;
			}
		}
		else if (ret) {
			return ret;
		}
	}
	switch (mreq->op) {
	case MSTOR_OP_CREAT:
		// TODO: implement overwrite?
		return -EEXIST;
	case MSTOR_OP_OPEN:
		return mstor_do_open(mstor, mreq, cnode);
	case MSTOR_OP_CHUNKFIND:
		return mstor_do_chunkfind(mstor, mreq, cnode);
	case MSTOR_OP_MKDIRS:
		return 0;
	case MSTOR_OP_LISTDIR:
		return mstor_do_listdir(mstor, mreq, cnode);
	case MSTOR_OP_STAT:
		return mstor_do_stat(mstor, mreq, pnode, cnode);
	case MSTOR_OP_CHMOD:
		return mstor_do_chmod(mstor, mreq, cnode);
	case MSTOR_OP_CHOWN:
		return mstor_do_chown(mstor, mreq, cnode);
	case MSTOR_OP_UTIMES:
		return mstor_do_utimes(mstor, mreq, cnode);
	case MSTOR_OP_RMDIR:
		return mstor_do_rmdir(mstor, mreq, pcomp, pnode, cnode);
	case MSTOR_OP_UNLINK:
		return mstor_do_unlink(mstor, mreq, pcomp, pnode, cnode);
	case MSTOR_OP_NODE_SEARCH: {
		struct mreq_node_search *req = (struct mreq_node_search*)mreq;
		req->npc_rem = 0;
		return 0;
	}
	default:
		abort();
		break;
	}
	/* unreachable, but keep compiler happy */
	return -ENOTSUP;
}

static int mstor_copy_last_pcomp(char *pcomp, const char *full_path)
{
	char *slash;

	slash = rindex(full_path, '/');
	if (!slash)
		return -EINVAL;
	if (slash[1] == '\0')
		return -ENOSYS;
	return zsnprintf(pcomp, RF_PCOMP_MAX, "%s", slash + 1);
}

static int mstor_do_rename(struct mstor *mstor, struct mreq *mreq)
{
	int ret;
	struct mreq_rename *req;
	struct mnode src_pnode, src_cnode;
	struct mnode dst_pnode, dst_cnode;
	struct mreq_node_search src_req, dst_req;
	uint16_t mode_and_type;
	uint64_t be_src_cnode_nid;
	char src_ckey[MCHILD_KEY_MAX], dst_ckey[MCHILD_KEY_MAX];
	char src_pcomp[RF_PCOMP_MAX], dst_pcomp[RF_PCOMP_MAX];
	char *err = NULL;
	leveldb_writebatch_t* bat = NULL;

	req = (struct mreq_rename*)mreq;
	memset(&src_pnode, 0, sizeof(src_pnode));
	memset(&src_cnode, 0, sizeof(src_cnode));
	memset(&dst_pnode, 0, sizeof(dst_pnode));
	memset(&dst_cnode, 0, sizeof(dst_cnode));
	memset(&src_req, 0, sizeof(src_req));
	memset(&dst_req, 0, sizeof(dst_req));
	mreq->user = udata_lookup_user(mstor->udata, mreq->user_name);
	if (IS_ERR(mreq->user)) {
		ret = -EUSERS;
		goto done;
	}
	src_req.base.op = MSTOR_OP_NODE_SEARCH;
	src_req.base.full_path = mreq->full_path;
	src_req.base.user_name = mreq->user_name;
	src_req.base.user = mreq->user;
	src_req.forbidden = RF_INVAL_NID;
	ret = mstor_do_path_operation(mstor, (struct mreq*)&src_req,
			&src_pnode, &src_cnode);
	if (ret)
		goto done;
	if (src_pnode.val == NULL) {
		/* Can't move the root directory to somewhere else */
		ret = -EINVAL;
	}
	mstor_copy_last_pcomp(src_pcomp, mreq->full_path);
	dst_req.base.op = MSTOR_OP_NODE_SEARCH;
	dst_req.base.full_path = req->dst_path;
	dst_req.base.user_name = mreq->user_name;
	dst_req.base.user = mreq->user;
	/* The path resolution of the destination should not cross the source.
	 * If it does, we are trying to make a directory a subdirectory of
	 * itself. */
	dst_req.forbidden = src_cnode.nid;
	ret = mstor_do_path_operation(mstor, (struct mreq*)&dst_req,
			&dst_pnode, &dst_cnode);
	if (ret == 0) {
		/* the target exists already */
		if (src_cnode.nid == dst_cnode.nid) {
			/* We are trying to move something to itself, which is
			 * stupid, but not actually forbidden. */
			return 0;
		}
		mode_and_type =
			unpack_from_be16(&dst_cnode.val->mode_and_type);
		if (!(mode_and_type & MNODE_IS_DIR)) {
			/* In contrast to POSIX, HDFS-style semantics make it an
			 * error to rename a file over an existing file.  We
			 * probably should eventually support a POSIX mode for
			 * renames. */
			ret = -EEXIST;
			goto done;
		}
		/* When asked to rename /a/b to /x/y, and y exists as a
		 * directory, we move the source to /x/y/a.  For HDFS, this
		 * succeeds whether the source is a file or a directory.  For
		 * POSIX, we would return -EISDIR here if the source was a
		 * regular file rather than a directory.  Rename semantics are
		 * weird. */
		mnode_free(&dst_pnode);
		memcpy(&dst_pnode, &dst_cnode, sizeof(struct mnode));
		memset(&dst_cnode, 0, sizeof(struct mnode));
		/* copy the last path component from the source path */
		mstor_copy_last_pcomp(dst_pcomp, mreq->full_path);
	}
	else if (ret == -ENOENT) {
		glitch_log("partial path components with npc_rem = %d\n", dst_req.npc_rem);
		/* We didn't resolve all path components.  But did we get all of
		 * them except the very last one?  If so, then we're right where
		 * we want to be.  If not, then we'd better return -ENOENT. */
		if (dst_req.npc_rem != 1)
			goto done;
		/* copy the last path component from the destination path */
		mstor_copy_last_pcomp(dst_pcomp, req->dst_path);
	}
	else {
		/* there was an error when looking up the target */
		goto done;
	}
	bat = leveldb_writebatch_create();
	if (!bat) {
		ret = -ENOMEM;
		goto done;
	}
	/* delete directory entry in src parent */
	src_ckey[0] = 'c';
	pack_to_be64(src_ckey + 1, src_pnode.nid);
	snprintf(src_ckey + 1 + sizeof(uint64_t), RF_PCOMP_MAX,
			"%s", src_pcomp);
	leveldb_writebatch_delete(bat, src_ckey,
			1 + sizeof(uint64_t) + strlen(src_pcomp));
	/* add directory entry in dst parent */
	dst_ckey[0] = 'c';
	pack_to_be64(dst_ckey + 1, dst_pnode.nid);
	snprintf(dst_ckey + 1 + sizeof(uint64_t), RF_PCOMP_MAX,
			"%s", dst_pcomp);
	pack_to_be64(&be_src_cnode_nid, src_cnode.nid);
	leveldb_writebatch_put(bat, dst_ckey,
			1 + sizeof(uint64_t) + strlen(dst_pcomp),
			(const char*)&be_src_cnode_nid, sizeof(uint64_t));
	leveldb_write(mstor->ldb, mstor->lwropt, bat, &err);
	if (err) {
		glitch_log("mstor_do_rename(src='%s',dst='%s'): got "
			"leveldb_write error '%s'\n",
			mreq->full_path, req->dst_path, err);
		ret = -EIO;
		goto done;
	}
	ret = 0;

done:
	free(err);
	if (bat)
		leveldb_writebatch_destroy(bat);
	mnode_free(&src_pnode);
	mnode_free(&src_cnode);
	mnode_free(&dst_pnode);
	mnode_free(&dst_cnode);
	return ret;
}

int mstor_do_operation(struct mstor *mstor, struct mreq *mreq)
{
	char lock_paths[4][RF_PATH_MAX + 1];
	int ret, rlocked;
	struct mnode pnode, cnode;

	/* Allocate space on the stack for the range lock paths */
	mreq->lk->range[0].start = lock_paths[0];
	mreq->lk->range[0].end = lock_paths[1];
	mreq->lk->range[1].start = lock_paths[2];
	mreq->lk->range[1].end = lock_paths[3];
	/* Take the range locks we need */
	rlocked = mstor_range_lock_by_op(mstor, mreq);
	switch (mreq->op) {
	case MSTOR_OP_CHUNKALLOC:
		ret = mstor_do_chunkalloc(mstor, mreq);
		break;
	case MSTOR_OP_FIND_ZOMBIES:
		ret = mstor_do_find_zombies(mstor, mreq);
		break;
	case MSTOR_OP_DESTROY_ZOMBIE:
		ret = mstor_do_destroy_zombie(mstor, mreq);
		break;
	case MSTOR_OP_RENAME:
		// TODO: handle cross-delegation renames
		ret = mstor_do_rename(mstor, mreq);
		break;
	default:
		mreq->user = udata_lookup_user(mstor->udata, mreq->user_name);
		if (IS_ERR(mreq->user)) {
			ret = -EUSERS;
			goto done;
		}
		memset(&pnode, 0, sizeof(pnode));
		memset(&cnode, 0, sizeof(cnode));
		ret = mstor_do_path_operation(mstor, mreq, &pnode, &cnode);
		mnode_free(&pnode);
		mnode_free(&cnode);
		break;
	}
done:
	if (rlocked)
		srange_unlock(mstor->tk, mreq->lk);
	glitch_log("mreq type %s returning result %d\n",
		mstor_op_ty_to_str(mreq->op), ret);
	return ret;
}

static int mstor_dump_child(FILE *out, const char *k, size_t klen,
		const char *v, size_t vlen)
{
	uint64_t pnid, cnid;
	char pcomp[RF_PATH_MAX];

	if ((klen <= (1 + sizeof(uint64_t))) ||
			(klen >= (1 + sizeof(uint64_t) + RF_PATH_MAX))) {
		glitch_log("mstor_dump: unknown key starting "
			"with 'c' of length %Zd\n", klen);
		return -EINVAL;
	}
	if (vlen != sizeof(uint64_t)) {
		glitch_log("mstor_dump: child entry has payload of "
			   "illegal length.  length = %Zd\n", vlen);
		return -EINVAL;
	}
	pnid = unpack_from_be64(k + 1);
	cnid = unpack_from_be64(v);
	memset(pcomp, 0, RF_PATH_MAX);
	memcpy(pcomp, k + 1 + sizeof(uint64_t), klen - 1 - sizeof(uint64_t));
	return zfprintf(out, "CHILD(0x%"PRIx64", %s) => 0x%"PRIx64"\n",
		pnid, pcomp, cnid);
}


static int mstor_dump_file_entry(FILE *out, const char *k, size_t klen,
		const char *v, size_t vlen)
{
	uint64_t nid, off, cid;

	if (klen != MFILE_KEY_LEN) {
		glitch_log("mstor_dump: unknown key starting "
			"with 'f' of length %Zd\n", klen);
		return -EINVAL;
	}
	if (vlen != sizeof(uint64_t)) {
		glitch_log("mstor_dump: file entry has payload of "
			   "illegal length.  length = %Zd\n", vlen);
		return -EINVAL;
	}
	nid = unpack_from_be64(k + 1);
	off = unpack_from_be64(k + sizeof(uint64_t) + 1);
	cid = unpack_from_be64(v);
	return zfprintf(out, "FILE(0x%"PRIx64", 0x%"PRIx64") => 0x%"PRIx64"\n",
		nid, off, cid);
}

static int mstor_dump_chunk_entry(FILE *out, const char *k, size_t klen,
		const char *v, size_t vlen)
{
	int num_oid, i;
	uint64_t cid;
	uint32_t oid;
	char buf[512];
	const char *prefix;
	size_t off;

	if (klen != MCHUNK_KEY_LEN) {
		glitch_log("mstor_dump: unknown key starting "
			   "with 'h' of length %Zd\n", klen);
		return -EINVAL;
	}
	if (vlen % sizeof(uint32_t)) {
		glitch_log("mstor_dump: invalid length for chunk mapping: "
			"expected multiple of %Zd, but got %Zd\n",
			sizeof(uint32_t), vlen);
		return -EINVAL;
	}
	cid = unpack_from_be64(k + 1);
	num_oid = vlen / sizeof(uint32_t);
	buf[0] = '\0';
	off = 0;
	prefix = "";
	for (i = 0; i < num_oid; ++i) {
		oid = unpack_from_be32(v + (i * sizeof(uint32_t)));
		fwdprintf(buf, &off, sizeof(buf), "%s%"PRIx32, prefix, oid);
		prefix = ", ";
	}
	return zfprintf(out, "CHUNK(0x%"PRIx64") => [ %s ]\n", cid, buf);
}

static int mstor_dump_node(FILE *out, const char *k, size_t klen,
		const char *v, size_t vlen)
{
	int is_dir;
	struct mnode_payload *hdr;
	uint16_t mode;
	uint32_t uid, gid;
	uint64_t nid, mtime, atime;

	if (klen != MNODE_KEY_LEN) {
		glitch_log("mstor_dump_node: unknown key starting "
			   "with 'n' of length %Zd\n", klen);
		return -EINVAL;
	}
	if (vlen < sizeof(struct mnode_payload)) {
		glitch_log("mstor_dump_node: node entry is too short to contain a "
			   "node header!  Length = %Zd\n", vlen);
		return -EINVAL;
	}
	if (vlen != sizeof(struct mnode_payload)) {
		glitch_log("mstor_dump_node: node entry is not equal to expected "
			   "length!  Length = %Zd\n", vlen);
		return -EINVAL;
	}
	nid = unpack_from_be64(k + 1);
	hdr = (struct mnode_payload*)v;
	mode = unpack_from_be16(&hdr->mode_and_type);
	is_dir = mode & MNODE_IS_DIR;
	mode &= ~MNODE_IS_DIR;
	mtime = unpack_from_be64(&hdr->mtime);
	atime = unpack_from_be64(&hdr->atime);
	uid = unpack_from_be32(&hdr->uid);
	gid = unpack_from_be32(&hdr->gid);
	return zfprintf(out, "NODE(0x%"PRIx64") => { ty=%s, mode=%04o, "
		"mtime=%"PRId64", atime=%"PRId64", uid='%"PRId32"', "
		"gid='%"PRId32"' }\n",
		nid, (is_dir ? "DIR" : "FILE"), mode,
		mtime, atime, uid,
		gid);
}

static int mstor_dump_zombie(FILE *out, const char *k, size_t klen,
		size_t vlen)
{
	uint64_t died, cid;

	if (klen != MZOMBIE_KEY_LEN) {
		glitch_log("mstor_dump_zombie: unknown key starting "
			   "with 'z' of length %Zd\n", klen);
		return -EINVAL;
	}
	if (vlen != 0) {
		glitch_log("mstor_dump_zombie: zombie entry has non-zero "
			   "payload length!  Length = %Zd\n", vlen);
		return -EINVAL;
	}
	died = unpack_from_be64(k + 1);
	cid = unpack_from_be64(k + 1 + sizeof(uint64_t));
	return zfprintf(out, "ZOMBIE(died=0x%"PRIx64", cid=0x%"PRIx64
			") => { }", died, cid);
}

int mstor_dump(struct mstor *mstor, FILE *out)
{
	int ret;
	char *err = NULL;
	const char *k;
	const char *v;
	size_t klen, vlen;
	leveldb_iterator_t *iter = NULL;
	uint32_t vers;

	iter = leveldb_create_iterator(mstor->ldb, mstor->lreadopt);
	if (!iter) {
		glitch_log("mstor_dump: leveldb_create_iterator failed.\n");
		ret = -ENOMEM;
		goto done;
	}
	leveldb_iter_seek_to_first(iter);
	while (1) {
		if (!leveldb_iter_valid(iter)) {
			break;
		}
		k = leveldb_iter_key(iter, &klen);
		v = leveldb_iter_value(iter, &vlen);
		if (klen < 1) {
			glitch_log("mstor_dump: leveldb_iter_key returned "
				"klen < 1.  That should not be "
				"possible.\n");
			ret = -EIO;
			goto done;
		}
		switch (k[0]) {
		case 'c':
			ret = mstor_dump_child(out, k, klen, v, vlen);
			if (ret)
				goto done;
			break;
		case 'f':
			ret = mstor_dump_file_entry(out, k, klen, v, vlen);
			if (ret)
				goto done;
			break;
		case 'h':
			ret = mstor_dump_chunk_entry(out, k, klen, v, vlen);
			if (ret)
				goto done;
			break;
		case 'n':
			ret = mstor_dump_node(out, k, klen, v, vlen);
			if (ret)
				goto done;
			break;
		case 'v':
			if (klen != 1) {
				glitch_log("mstor_dump: unknown key starting "
					   "with 'v' of length %Zd\n", klen);
				ret = -EIO;
				goto done;
			}
			vers = mstor_parse_version(v, vlen);
			if (vers == MSTOR_VERSION_INVAL) {
				ret = -EINVAL;
				goto done;
			}
			ret = zfprintf(out, "MSTOR_VERSION(%"PRId32")\n", vers);
			if (ret)
				goto done;
			break;
		case 'z':
			ret = mstor_dump_zombie(out, k, klen, vlen);
			if (ret)
				goto done;
			break;
		default:
			glitch_log("mstor_dump: found key of unknown "
				   "type '%c'\n", k[0]);
			ret = -EIO;
			goto done;
		}
		leveldb_iter_next(iter);
	}
	ret = 0;

done:
	free(err);
	if (iter)
		leveldb_iter_destroy(iter);
	return ret;
}
