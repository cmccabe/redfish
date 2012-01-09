/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "common/config/mstorc.h"
#include "core/glitch_log.h"
#include "jorm/jorm_const.h"
#include "mds/limits.h"
#include "mds/mstor.h"
#include "msg/generic.h"
#include "util/error.h"
#include "util/fast_log.h"
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
 * for file chunks:
 *      f[8-byte node-id][8-byte offset] => 8-byte chunk ID
 * for directory children:
 *	c[8-byte node-id][child-name] => 8-byte child ID
 * for chunks:
 *      h[8-byte-chunk-id] => <array of 4-byte OSD-IDs>
 * for trashed (sequestered) files:
 *      t[8-byte-expiry-time][8-byte-node-id] => mnode
 */
/****************************** constants ********************************/
#define MSTOR_DEFAULT_SEQUESTER_TIME 300

#define MSTOR_CUR_VERSION 0x000000001U
#define MSTOR_VERSION_MAGIC "Fish"
#define MSTOR_VERSION_MAGIC_LEN 4
#define MSTOR_VERSION_BODY_LEN 8
#define MSTOR_VERSION_INVAL 0xffffffffU

#define MSTOR_ROOT_NID 0
#define MSTOR_ROOT_NID_INIT_MODE (0755 | MNODE_IS_DIR)

#define MSTOR_PERM_EXEC 01
#define MSTOR_PERM_WRITE 02
#define MSTOR_PERM_READ 04

#define MSTOR_NID_MAX 0xffffffffffff0000ULL
#define MNODE_KEY_LEN (1 + sizeof(uint64_t))
#define MCHILD_KEY_LEN_PREFIX (1 + sizeof(uint64_t))
#define MCHUNK_KEY_LEN (1 + sizeof(uint64_t))
#define MFILE_KEY_LEN (1 + sizeof(uint64_t) + sizeof(uint64_t))
#define MCHILD_KEY_MAX (1 + sizeof(uint64_t) + RF_PCOMP_MAX)

#define MREQ_FLAG_CHECK_PERMS 0x1

/****************************** prototypes ********************************/
static void mstor_leveldb_shutdown(struct mstor *mstor);

/****************************** types ********************************/
/** A metadata node representing either a file or a directory
 */
struct mnode {
	/** Node id */
	uint64_t nid;
	/** Pointer to data record */
	struct mnode_payload *val;
};

PACKED(
struct mnode_payload {
	uint16_t mode_and_type;
	uint64_t mtime;
	uint64_t atime;
	uint32_t uid;
	uint32_t gid;
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
	int min_sequester_time;
	/** Protects next_nid */
	pthread_mutex_t next_nid_lock;
};

/****************************** functions ********************************/
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
	case MSTOR_OP_SEQUESTER:
		return "MSTOR_OP_SEQUESTER";
	case MSTOR_OP_SEQUESTER_TREE:
		return "MSTOR_OP_SEQUESTER_TREE";
	case MSTOR_OP_FIND_SEQUESTERED:
		return "MSTOR_OP_FIND_SEQUESTERED";
	case MSTOR_OP_DESTROY_SEQUESTERED:
		return "MSTOR_OP_DESTROY_SEQUESTERED";
	case MSTOR_OP_RENAME:
		return "MSTOR_OP_RENAME";
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
	if (err)
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
	if (err)
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
	pack_to_be16(&hdr->mode_and_type, MSTOR_ROOT_NID_INIT_MODE);
	t = time(NULL);
	pack_to_be64(&hdr->mtime, t);
	pack_to_be64(&hdr->atime, t);
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
	__sync_synchronize();
	ret = 0;

done:
	if (err)
		free(err);
	if (iter)
		leveldb_iter_destroy(iter);
	return ret;
}

static int mstor_leveldb_load(struct mstor *mstor)
{
	int ret;
	leveldb_iterator_t *iter = NULL;
	size_t klen;
	char *err = NULL, nkey[MNODE_KEY_LEN];
	const char *key;
	uint64_t next_nid;
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
	nkey[0] = 'n';
	pack_to_be64(nkey + 1, MSTOR_NID_MAX);
	leveldb_iter_seek(iter, nkey, MNODE_KEY_LEN);
	leveldb_iter_prev(iter);
	if (!leveldb_iter_valid(iter)) {
		glitch_log("mstor_leveldb_load: failed to seek to highest "
			   "node id.\n");
		ret = -EINVAL;
		goto done;
	}
	key = leveldb_iter_key(iter, &klen);
	if ((klen != MNODE_KEY_LEN) || (key[0] != 'n')) {
		glitch_log("mstor_leveldb_setup: failed to find "
			"highest node ID in use\n");
		ret = -EINVAL;
		goto done;
	}
	next_nid = unpack_from_be64(key + 1) + 1;
	mstor->next_nid = next_nid;
	__sync_synchronize();
	glitch_log("mstor_leveldb_setup: using existing mstor.  "
		   "Next nid = 0x%"PRIx64"\n", next_nid);
	ret = 0;

done:
	if (err)
		free(err);
	if (iter)
		leveldb_iter_destroy(iter);
	return ret;
}

static int mstor_mode_check(const struct mnode *node,
			struct mreq *mreq, int want)
{
	int uid_match = 0, group_match = 0;
	uint16_t mode;
	uint32_t uid, gid;

	mode = unpack_from_be16(&node->val->mode_and_type);
	if (want & MNODE_IS_DIR) {
		if ((mode & MNODE_IS_DIR) == 0)
			return -ENOTDIR;
	}
	else {
		if (mode & MNODE_IS_DIR)
			return -EISDIR;
	}
	if (!(mreq->flags & MREQ_FLAG_CHECK_PERMS)) {
		/* skip permission check */
		return 0;
	}
	mode &= ~MNODE_IS_DIR;
	want &= ~MNODE_IS_DIR;
	/* Check whether everyone has the permission we seek */
	if ((want << 6) & mode)
		return 0;
	uid = unpack_from_be32(&node->val->uid);
	if (uid == mreq->uid) {
		/* Check whether the owner has the permission we seek */
		uid_match = 1;
		if (want & mode)
			return 0;
	}
	gid = unpack_from_be32(&node->val->gid);
	if (gid == mreq->gid) { // FIXME: should check for group membership
		/* Check whether the group has the permission we seek */
		group_match = 1;
		if ((want << 3) & mode)
			return 0;
	}
	glitch_log("mstor_mode_check(want=%02o, nid=0x%"PRIx64", "
			"mode=%04o): returning -EPERM\n",
			want, node->nid, mode);
	return -EPERM;
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

	lopt = leveldb_options_create();
	if (!lopt) {
		ret = -ENOMEM;
		goto error;
	}
	leveldb_options_set_create_if_missing(lopt, (conf->mstor_create != 0));
	leveldb_options_set_compression(lopt, leveldb_no_compression);
	lcache = leveldb_cache_create_lru(conf->mstor_cache_size);
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
	if (conf->min_sequester_time == JORM_INVAL_INT)
		mstor->min_sequester_time = MSTOR_DEFAULT_SEQUESTER_TIME;
	else
		mstor->min_sequester_time = conf->min_sequester_time;
	return 0;

error:
	if (err)
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
		const struct mstorc *conf)
{
	int ret;
	struct mstor *mstor;

	mstor = calloc(1, sizeof(struct mstor));
	if (!mstor) {
		ret = ENOMEM;
		goto error;
	}
	ret = pthread_mutex_init(&mstor->next_nid_lock, NULL);
	if (ret)
		goto error_free_mstor;
	ret = mstor_leveldb_init(mstor, conf);
	if (ret)
		goto error_destroy_next_nid_lock;
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
	free(mstor);
}

static int mstor_fetch_node(struct mstor *mstor, uint64_t nid,
			struct mnode *node)
{
	char *val, *err = NULL;
	size_t vlen;
	char key[MNODE_KEY_LEN];

	key[0] = 'n';
	pack_to_be64(key + 1, nid);
	val = leveldb_get(mstor->ldb, mstor->lreadopt, key, MNODE_KEY_LEN,
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
	char key[MCHILD_KEY_MAX];
	char *val, *err = NULL;
	size_t klen, vlen;
	uint64_t cnid;

	/* Do we have the permission to look up this child? */
	ret = mstor_mode_check(pnode, mreq,
			MSTOR_PERM_EXEC | MNODE_IS_DIR);
	if (ret)
		return ret;
	/* Look up the child nid */
	key[0] = 'c';
	pack_to_be64(key + 1, pnode->nid);
	snprintf(key + 1 + sizeof(uint64_t), RF_PCOMP_MAX,
		"%s", pcomp);
	klen = 1 + sizeof(uint64_t) + strlen(pcomp);
	val = leveldb_get(mstor->ldb, mstor->lreadopt, key,
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
	char pkey[MCHILD_KEY_MAX], nkey[MNODE_KEY_LEN];
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
	pkey[0] = 'c';
	pack_to_be64(pkey + 1, pnode->nid);
	snprintf(pkey + 1 + sizeof(uint64_t), RF_PCOMP_MAX,
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
	leveldb_writebatch_put(bat, pkey, plen, nkey + 1, sizeof(uint64_t));
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

	req = (struct mreq_creat*)mreq;
	ret = mstor_make_node(mstor, req->mode, req->ctime, req->ctime,
		mreq->uid, mreq->gid, pcomp, pnode, cnode);
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
	// TODO: release lock here
	return ret;
}

static int mstor_do_mkdir(struct mstor *mstor, struct mreq *mreq,
		const char *pcomp, const struct mnode *pnode,
		struct mnode *cnode)
{
	int ret;
	struct mreq_mkdirs *req;

	req = (struct mreq_mkdirs*)mreq;
	ret = mstor_make_node(mstor, req->mode | MNODE_IS_DIR,
		req->ctime, req->ctime, mreq->uid,
		mreq->gid, pcomp, pnode, cnode);
	return ret;
}

static int add_stat_to_list(uint32_t *off, uint32_t out_len,
		const char *pcomp, const struct mnode *node, char *out)
{
	int ret;
	struct mmm_stat_hdr *hdr;
	uint32_t old, o;
	uint32_t stat_len;

	old = o = *off;
	if ((out_len - o) < sizeof(struct mmm_stat_hdr)) {
		return -ENAMETOOLONG;
	}
	hdr = (struct mmm_stat_hdr*)(out + o);
	hdr->mode_and_type = node->val->mode_and_type;
	hdr->block_sz = 0; // TODO: fill in
	hdr->mtime = node->val->mtime;
	hdr->atime = node->val->atime;
	hdr->length = 0; // TODO: needs to be calculated by looking at chunks
			// for this nid
	pack_to_8(&hdr->repl, 1); // TODO: this ought to be the 'target' value in
					// mnode_payload?
	o += sizeof(struct mmm_stat_hdr);
	/* path name */
	ret = pack_str(out, &o, out_len, pcomp);
	if (ret)
		return ret;
	/* owner */
	hdr->uid = node->val->uid;
	/* group */
	hdr->gid = node->val->gid;
	/* success */
	stat_len = o - old;
	if (stat_len > 0xffff)
		return -ENAMETOOLONG;
	*off = o;
	pack_to_be16(&hdr->stat_len, (uint16_t)stat_len);
	return 0;
}

static int mstor_do_listdir(struct mstor *mstor, struct mreq *mreq,
		const struct mnode *dnode)
{
	int ret;
	char *err = NULL;
	leveldb_iterator_t *iter = NULL;
	const char *k;
	const char *v;
	char ikey[1 + sizeof(uint64_t)], pcomp[RF_PCOMP_MAX];
	size_t klen, vlen;
	struct mnode node;
	uint32_t off;
	uint64_t nid;
	struct mreq_listdir *req;

	req = (struct mreq_listdir*)mreq;
	req->used_len = 0;
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
	ikey[0] = 'c';
	pack_to_be64(ikey + 1, dnode->nid);
	leveldb_iter_seek(iter, ikey, sizeof(ikey));
	off = 0;
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
		ret = add_stat_to_list(&off, req->out_len, pcomp,
				&node, req->out);
		if (ret)
			goto done;
next:
		mnode_free(&node);
		memset(&node, 0, sizeof(struct mnode));
		leveldb_iter_next(iter);
	}
	req->used_len = off;
	ret = 0;
done:
	if (err)
		free(err);
	if (iter)
		leveldb_iter_destroy(iter);
	mnode_free(&node);
	return ret;
}

static int mstor_do_stat(struct mreq *mreq, const char *pcomp,
		const struct mnode *pnode, const struct mnode *cnode)
{
	int ret;
	struct mreq_stat *req;
	uint32_t off = 0;

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
	req = (struct mreq_stat*)mreq;
	ret = add_stat_to_list(&off, req->out_len, pcomp, cnode, req->out);
	return ret;
}

static int mstor_do_chmod(struct mstor *mstor, struct mreq *mreq,
		const struct mnode *node)
{
	int ret;
	struct mreq_chmod *req;
	struct mnode_payload *hdr;
	char k[MNODE_KEY_LEN], *err = NULL;
	uint16_t old_mode_and_type, mode_and_type;

	// TODO: take lock here
	req = (struct mreq_chmod*)mreq;
	hdr = (struct mnode_payload*)node->val;
	mode_and_type = req->mode;
	old_mode_and_type = unpack_from_be16(&hdr->mode_and_type);
	if (old_mode_and_type & MNODE_IS_DIR)
		mode_and_type |= MNODE_IS_DIR;
	else
		mode_and_type &= ~MNODE_IS_DIR;
	pack_to_be16(&hdr->mode_and_type, mode_and_type);
	k[0] = 'n';
	pack_to_be64(k + 1, node->nid);
	leveldb_put(mstor->ldb, mstor->lwropt, k, MNODE_KEY_LEN,
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
	// TODO: release lock here
	return ret;
}

static int mstor_do_chown(struct mstor *mstor, struct mreq *mreq,
		const struct mnode *node)
{
	int ret;
	struct mreq_chown *req;
	char k[MNODE_KEY_LEN], *err = NULL;
	char buf[sizeof(struct mnode_payload)];

	// TODO: take lock here
	req = (struct mreq_chown*)mreq;
	if (req->new_uid != RF_INVAL_UID) {
		pack_to_be32(&node->val->uid, req->new_uid);
	}
	if (req->new_gid != RF_INVAL_GID) {
		pack_to_be32(&node->val->gid, req->new_gid);
	}
	memcpy(buf, node->val, sizeof(struct mnode_payload));
	k[0] = 'n';
	pack_to_be64(k + 1, node->nid);
	leveldb_put(mstor->ldb, mstor->lwropt, k, MNODE_KEY_LEN,
			buf, sizeof(struct mnode_payload), &err);
	if (err) {
		glitch_log("mstor_do_chown(nid=0x%"PRIx64": leveldb_put "
			"returned error '%s'\n", node->nid, err);
		ret = -EIO;
		goto done;
	}
	ret = 0;

done:
	free(err);
	// TODO: release lock here
	return ret;
}

static int mstor_do_utimes(struct mstor *mstor, struct mreq *mreq,
		const struct mnode *node)
{
	int ret;
	struct mreq_utimes *req;
	struct mnode_payload *hdr;
	char k[MNODE_KEY_LEN], *err = NULL;

	// TODO: take lock here
	req = (struct mreq_utimes*)mreq;
	hdr = (struct mnode_payload*)node->val;
	if (req->atime != RF_INVAL_TIME)
		pack_to_be64(&hdr->atime, req->atime);
	if (req->mtime != RF_INVAL_TIME)
		pack_to_be64(&hdr->mtime, req->mtime);
	k[0] = 'n';
	pack_to_be64(k + 1, node->nid);
	leveldb_put(mstor->ldb, mstor->lwropt, k, MNODE_KEY_LEN,
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
	// TODO: release lock here
	return ret;
}

static int mstor_do_operation_impl(struct mstor *mstor, struct mreq *mreq,
			    struct mnode *pnode, struct mnode *cnode)
{
	char *pcomp;
	int ret, npc, cpc;
	char full_path[RF_PATH_MAX];

	mreq->flags = MREQ_FLAG_CHECK_PERMS;
	/* The superuser can do anything */
	if (mreq->uid == RF_SUPERUSER_UID)
		mreq->flags &= ~MREQ_FLAG_CHECK_PERMS;
	if (mreq->gid == RF_SUPERUSER_GID)
		mreq->flags &= ~MREQ_FLAG_CHECK_PERMS;
	if (zsnprintf(full_path, sizeof(full_path), "%s", mreq->full_path))
		return -ENAMETOOLONG;
	ret = canonicalize_path(full_path);
	if (ret)
		return ret;
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
	for (cpc = 0; cpc < npc; ++cpc) {
		mnode_free(pnode);
		memcpy(pnode, cnode, sizeof(struct mnode));
		memset(cnode, 0, sizeof(struct mnode));
		pcomp = memchr(pcomp, '\0', RF_PATH_MAX) + 1;
		// TODO: lock if this is a write operation, and this is the
		// last path component.  Or if this is a mkdirs operation.
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
		return -ENOTSUP;
	case MSTOR_OP_CHUNKALLOC:
		return -ENOTSUP;
	case MSTOR_OP_MKDIRS:
		return 0;
	case MSTOR_OP_LISTDIR:
		return mstor_do_listdir(mstor, mreq, cnode);
	case MSTOR_OP_STAT:
		return mstor_do_stat(mreq, pcomp, pnode, cnode);
	case MSTOR_OP_CHMOD:
		return mstor_do_chmod(mstor, mreq, cnode);
	case MSTOR_OP_CHOWN:
		return mstor_do_chown(mstor, mreq, cnode);
	case MSTOR_OP_UTIMES:
		return mstor_do_utimes(mstor, mreq, cnode);
	case MSTOR_OP_SEQUESTER:
		return -ENOTSUP;
	case MSTOR_OP_SEQUESTER_TREE:
		return -ENOTSUP;
	case MSTOR_OP_FIND_SEQUESTERED:
		return -ENOTSUP;
	case MSTOR_OP_DESTROY_SEQUESTERED:
		return -ENOTSUP;
	case MSTOR_OP_RENAME:
		return -ENOTSUP;
	default:
		abort();
		break;
	}
	/* unreachable, but keep compiler happy */
	return -ENOTSUP;
}

int mstor_do_operation(struct mstor *mstor, struct mreq *mreq)
{
	int ret;
	struct mnode pnode, cnode;

	memset(&pnode, 0, sizeof(pnode));
	memset(&cnode, 0, sizeof(cnode));
	ret = mstor_do_operation_impl(mstor, mreq, &pnode, &cnode);
	glitch_log("mreq type %s returning result %d\n",
		mstor_op_ty_to_str(mreq->op), ret);
	mnode_free(&pnode);
	mnode_free(&cnode);
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

static int mstor_dump_node(FILE *out, const char *k, size_t klen,
		const char *v, size_t vlen)
{
	int is_dir;
	struct mnode_payload *hdr;
	uint16_t mode;
	uint32_t uid, gid;
	uint64_t nid, mtime, atime;

	if (klen != MNODE_KEY_LEN) {
		glitch_log("mstor_dump: unknown key starting "
			   "with 'n' of length %Zd\n", klen);
		return -EINVAL;
	}
	if (vlen < sizeof(struct mnode_payload)) {
		glitch_log("mstor_dump: node entry is too short to contain a "
			   "node header!  Length = %Zd\n", vlen);
		return -EINVAL;
	}
	if (vlen != sizeof(struct mnode_payload)) {
		glitch_log("mstor_dump: node entry is not equal to expected "
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
			if (klen != MFILE_KEY_LEN) {
				glitch_log("mstor_dump: unknown key starting "
					   "with 'f' of length %Zd\n", klen);
				ret = -EIO;
				goto done;
			}
			break;
		case 'h':
			if (klen != MCHUNK_KEY_LEN) {
				glitch_log("mstor_dump: unknown key starting "
					   "with 'h' of length %Zd\n", klen);
				ret = -EIO;
				goto done;
			}
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
	if (err)
		free(err);
	if (iter)
		leveldb_iter_destroy(iter);
	return ret;
}
