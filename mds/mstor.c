/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "common/config/mstorc.h"
#include "core/glitch_log.h"
#include "mds/limits.h"
#include "mds/mstor.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/packed.h"
#include "util/path.h"
#include "util/simple_io.h"
#include "util/string.h"

#include <errno.h>
#include <inttypes.h>
#include <leveldb/c.h>
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
 */
/****************************** constants ********************************/
#define MSTOR_CUR_VERSION 0x000000001U
#define MSTOR_VERSION_MAGIC "Fish"
#define MSTOR_VERSION_MAGIC_LEN 4
#define MSTOR_VERSION_BODY_LEN 8
#define MSTOR_VERSION_INVAL 0xffffffffU

#define MSTOR_ROOT_NID 0
#define MSTOR_ROOT_NID_INIT_MODE (0755 | MNODE_IS_DIR)

#define MNODE_IS_DIR 0x8000
#define MSTOR_PERM_EXEC 01
#define MSTOR_PERM_WRITE 02
#define MSTOR_PERM_READ 04

#define MNODE_MAX_SZ (sizeof(struct mnode_hdr) + RF_USER_MAX + RF_GROUP_MAX)
#define MSTOR_NID_MAX 0xffffffffffff0000ULL
#define MNODE_KEY_LEN (1 + sizeof(uint64_t))
#define MCHUNK_KEY_LEN (1 + sizeof(uint64_t))
#define MFILE_KEY_LEN (1 + sizeof(uint64_t) + sizeof(uint64_t))
#define MNODE_BODY_MAX (sizeof(struct mnode_hdr) + RF_USER_MAX + 1 + \
			RF_GROUP_MAX + 1)
#define MCHILD_KEY_MAX (1 + sizeof(uint64_t) + RF_PATH_COMPONENT_MAX)

#define MREQ_FLAG_CHECK_PERMS 0x1

/****************************** prototypes ********************************/

/****************************** types ********************************/
/** A metadata node representing either a file or a directory
 */
struct mnode {
	/** Node id */
	uint64_t nid;
	/** Pointer to data record */
	struct mnode_hdr *val;
	/** Length of data record */
	size_t len;
};

PACKED(
struct mnode_hdr {
	uint16_t mode_and_type;
	int64_t mtime;
	int64_t atime;
	char data[0];
	/* owner (cstr) */
	/* group (cstr) */
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
	case MSTOR_OP_UNLINK:
		return "MSTOR_OP_UNLINK";
	case MSTOR_OP_UNLINK_TREE:
		return "MSTOR_OP_UNLINK_TREE";
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
 */
static uint64_t mstor_next_nid(struct mstor *mstor)
{
	uint64_t nid;

	nid = __sync_fetch_and_add(&mstor->next_nid, 1);
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
	char nkey[MNODE_KEY_LEN], nbody[MNODE_BODY_MAX];
	struct mnode_hdr *hdr;
	uint32_t off;
	uint64_t t;

	glitch_log("mstor_leveldb_setup: setting up new mstor\n");
	ret = mstor_write_version(mstor, MSTOR_CUR_VERSION);
	if (ret)
		goto done;
	nkey[0] = 'n';
	pack_to_be64(nkey + 1, MSTOR_ROOT_NID);
	hdr = (struct mnode_hdr*)nbody;
	pack_to_be16(&hdr->mode_and_type, MSTOR_ROOT_NID_INIT_MODE);
	t = time(NULL);
	pack_to_be64(&hdr->mtime, t);
	pack_to_be64(&hdr->atime, t);
	off = offsetof(struct mnode_hdr, data);
	ret = pack_str(nbody, &off, sizeof(nbody), RF_SUPERUSER);
	if (ret)
		goto done;
	ret = pack_str(nbody, &off, sizeof(nbody), RF_SUPERUSER);
	if (ret)
		goto done;
	leveldb_put(mstor->ldb, mstor->lwropt, nkey, MNODE_KEY_LEN,
			nbody, off, &err);
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
		   "Highest nid = 0x%"PRIx64"\n", next_nid);
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
	int ret, user_match = 0, group_match = 0;
	uint16_t mode;
	uint32_t off;
	char owner[RF_USER_MAX], group[RF_GROUP_MAX];

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
	/* Check whether everyone has the permission we seek */
	if ((want << 6) & mode)
		return 0;
	off = offsetof(struct mnode_hdr, data);
	ret = unpack_str(node->val, &off, node->len, owner, sizeof(owner));
	if (ret) {
		glitch_log("mstor_mode_check(nid=0x%"PRIx64"): error "
			   "unpacking owner string\n", node->nid);
		return -EIO;
	}
	if (strcmp(mreq->user, owner)) {
		/* Check whether the owner has the permission we seek */
		user_match = 1;
		if (want & mode)
			return 0;
	}
	ret = unpack_str(node->val, &off, node->len, group, sizeof(group));
	if (ret) {
		glitch_log("mstor_mode_check(nid=0x%"PRIx64"): error "
			   "unpacking group string\n", node->nid);
		return -EIO;
	}
	if (strcmp(mreq->group, group)) {
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
	ret = mstor_leveldb_init(mstor, conf);
	if (ret)
		goto error;
	ret = mstor_leveldb_is_empty(mstor);
	if (ret < 0)
		goto error;
	else if (ret == 1) {
		ret = mstor_leveldb_create_new(mstor);
		if (ret)
			goto error;
	}
	else {
		ret = mstor_leveldb_load(mstor);
		if (ret)
			goto error;
	}
	return mstor;

error:
	free(mstor);
	glitch_log("mstor_init failed with error %d\n", ret);
	return ERR_PTR(FORCE_POSITIVE(ret));
}

void mstor_shutdown(struct mstor *mstor)
{
	glitch_log("mstor_shutdown: shutting down mstor\n");
	leveldb_readoptions_destroy(mstor->lreadopt);
	leveldb_writeoptions_destroy(mstor->lwropt);
	leveldb_cache_destroy(mstor->lcache);
	leveldb_close(mstor->ldb);
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
	node->nid = nid;
	node->val =  (struct mnode_hdr *)val;
	node->len = vlen;
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

	if (mreq->flags & MREQ_FLAG_CHECK_PERMS) {
		/* Do we have the permission to look up this child? */
		ret = mstor_mode_check(pnode, mreq,
				MSTOR_PERM_EXEC | MNODE_IS_DIR);
		if (ret)
			return ret;
	}
	/* Look up the child nid */
	key[0] = 'c';
	pack_to_be64(key + 1, pnode->nid);
	snprintf(key + 1 + sizeof(uint64_t), RF_PATH_COMPONENT_MAX,
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
	uint64_t mtime, uint64_t atime, const char *user, const char *group,
	const char *pcomp, const struct mnode *pnode, struct mnode *cnode)
{
	int ret;
	uint64_t cnid;
	leveldb_writebatch_t* bat = NULL;
	char pkey[MCHILD_KEY_MAX], nkey[MNODE_KEY_LEN];
	char *body = NULL, *err = NULL;
	size_t plen, body_len;
	struct mnode_hdr *hdr;
	uint32_t off;

	cnid = mstor_next_nid(mstor);
	if (cnid == MSTOR_NID_MAX) {
		ret =-EOVERFLOW;
		goto error;
	}
	body_len = sizeof(struct mnode_hdr) + strlen(user) + 1 +
			strlen(group) + 1;
	body = calloc(1, body_len);
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
	snprintf(pkey + 1 + sizeof(uint64_t), RF_PATH_COMPONENT_MAX,
		"%s", pcomp);
	plen = 1 + sizeof(uint64_t) + strlen(pcomp);
	nkey[0] = 'n';
	pack_to_be64(nkey + 1, cnid);
	hdr = (struct mnode_hdr*)body;
	pack_to_be16(&hdr->mode_and_type, mode_and_type);
	pack_to_be64(&hdr->mtime, mtime);
	pack_to_be64(&hdr->atime, atime);
	off = offsetof(struct mnode_hdr, data);
	ret = pack_str(body, &off, body_len, user);
	if (ret)
		goto error;
	ret = pack_str(body, &off, body_len, group);
	if (ret)
		goto error;
	leveldb_writebatch_put(bat, pkey, plen, nkey, sizeof(uint64_t));
	leveldb_writebatch_put(bat, nkey, MNODE_KEY_LEN, body, off);
	leveldb_write(mstor->ldb, mstor->lwropt, bat, &err);
	if (err) {
		glitch_log("leveldb_write(%" PRIx64 ") returned error '%s'\n",
			cnid, err);
		ret = -EIO;
		goto error;
	}
	cnode->nid = cnid;
	cnode->val = (struct mnode_hdr*)body;
	cnode->len = off;
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
	ret = mstor_make_node(mstor, req->mode, req->mtime, req->mtime,
		mreq->user, mreq->group, pcomp, pnode, cnode);
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
		req->mtime, req->mtime, mreq->user,
		mreq->group, pcomp, pnode, cnode);
	return ret;
}

static int mstor_do_operation_impl(struct mstor *mstor, struct mreq *mreq,
			    struct mnode *pnode, struct mnode *cnode)
{
	char *pcomp;
	int ret, npc, cpc;
	char full_path[RF_PATH_MAX];

	mreq->flags = MREQ_FLAG_CHECK_PERMS;
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
	pcomp = full_path;
	cpc = 0;
	ret = mstor_fetch_node(mstor, MSTOR_ROOT_NID, pnode);
	if (ret) {
		glitch_log("mstor_do_operation: couldn't load "
			"root node! Error %d\n", ret);
		return -ENOSYS;
	}
	for (cpc = 0; cpc < npc; ++cpc) {
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
		pcomp = memchr(pcomp, '\0', RF_PATH_MAX);
		mnode_free(pnode);
		memcpy(pnode, cnode, sizeof(struct mnode));
		memset(cnode, 0, sizeof(struct mnode));
	}
	switch (mreq->op) {
	case MSTOR_OP_CREAT:
		// TODO: implement overwrite?
		return -EEXIST;
	case MSTOR_OP_OPEN:
		break;
	case MSTOR_OP_CHUNKFIND:
		return -ENOTSUP;
	case MSTOR_OP_CHUNKALLOC:
		return -ENOTSUP;
	case MSTOR_OP_MKDIRS:
		return 0;
	case MSTOR_OP_LISTDIR:
		return -ENOTSUP;
	case MSTOR_OP_STAT:
		return -ENOTSUP;
	case MSTOR_OP_CHMOD:
		return -ENOTSUP;
	case MSTOR_OP_CHOWN:
		return -ENOTSUP;
	case MSTOR_OP_UTIMES:
		return -ENOTSUP;
	case MSTOR_OP_UNLINK:
		return -ENOTSUP;
	case MSTOR_OP_UNLINK_TREE:
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
	pnid = unpack_from_be64(k);
	cnid = unpack_from_be64(v);
	memset(pcomp, 0, RF_PATH_MAX);
	memcpy(pcomp, k + 1 + sizeof(uint64_t), klen - 1 - sizeof(uint64_t));
	return zfprintf(out, "CHILD(0x%"PRIx64", %s) => 0x%"PRIx64"\n",
		pnid, pcomp, cnid);
}

static int mstor_dump_node(FILE *out, const char *k, size_t klen,
		const char *v, size_t vlen)
{
	int ret, is_dir;
	struct mnode_hdr *hdr;
	uint16_t mode;
	uint32_t off;
	uint64_t nid, mtime, atime;
	char owner[RF_USER_MAX], group[RF_GROUP_MAX];

	if (klen != MNODE_KEY_LEN) {
		glitch_log("mstor_dump: unknown key starting "
			   "with 'n' of length %Zd\n", klen);
		return -EINVAL;
	}
	if (vlen < sizeof(struct mnode_hdr)) {
		glitch_log("mstor_dump: node entry is too short to contain a "
			   "node header!  Length = %Zd\n", vlen);
		return -EINVAL;
	}
	if (vlen > MNODE_BODY_MAX) {
		glitch_log("mstor_dump: node entry is longer than maximum "
			   "length!  Length = %Zd\n", vlen);
		return -EINVAL;
	}
	nid = unpack_from_be64(k);
	hdr = (struct mnode_hdr*)v;
	mode = unpack_from_be16(&hdr->mode_and_type);
	is_dir = mode & MNODE_IS_DIR;
	mode &= ~MNODE_IS_DIR;
	mtime = unpack_from_be64(&hdr->mtime);
	atime = unpack_from_be64(&hdr->atime);
	off = offsetof(struct mnode_hdr, data);
	ret = unpack_str(v, &off, vlen, owner, sizeof(owner));
	if (ret) {
		glitch_log("mstor_dump: error unpacking owner string\n");
		return -EINVAL;
	}
	ret = unpack_str(v, &off, vlen, group, sizeof(group));
	if (ret) {
		glitch_log("mstor_dump: error unpacking group string\n");
		return -EINVAL;
	}
	return zfprintf(out, "NODE(0x%"PRIx64") => { ty=%s, mode=%04o, "
		"mtime=%"PRId64", atime=%"PRId64", owner='%s', "
		"group='%s' }\n",
		nid, (is_dir ? "DIR" : "FILE"), mode,
		mtime, atime, owner,
		group);
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
