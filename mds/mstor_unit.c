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
#include "core/process_ctx.h"
#include "mds/const.h"
#include "mds/mstor.h"
#include "mds/srange_lock.h"
#include "mds/user.h"
#include "msg/xdr.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/packed.h"
#include "util/path.h"
#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSTORU_NUM_IO_THREADS 5

#define MSTORU_SUPER_USER "superuser"

#define MSTORU_SPOONY_USER "spoony"
#define MSTORU_SPOONY_UID 2

#define MSTORU_WOOT_USER "woot"
#define MSTORU_WOOT_UID 3

#define MSTORU_USERS_GROUP "users"
#define MSTORU_USERS_GID 2

#define MSTORU_WOOTERS_GROUP "wooters"
#define MSTORU_WOOTERS_GID 3

#define MSTORU_INVALID_USER "invalid_user"

#define MSTORU_MAX_NID 32
#define MSTORU_MAX_CINFOS 64
#define MSTORU_MAX_ZINFOS 64

static pthread_key_t g_tls_key;

struct mstoru_tls {
	sem_t sem;
	struct srange_locker *lk;
};

static struct mstoru_tls* mstoru_tls_get(void)
{
	struct mstoru_tls *tls;

	tls = pthread_getspecific(g_tls_key);
	if (tls)
		return tls;
	tls = calloc(1, sizeof(struct mstoru_tls));
	if (!tls)
		abort();
	if (sem_init(&tls->sem, 0, 0))
		abort();
	tls->lk = calloc(1, sizeof(struct srange_locker));
	if (!tls->lk)
		abort();
	tls->lk->sem = &tls->sem;
	if (pthread_setspecific(g_tls_key, tls))
		abort();
	return tls;
}

static void mstoru_tls_destroy(void *v)
{
	struct mstoru_tls *tls = (struct mstoru_tls*)v;

	free(tls->lk);
	sem_destroy(&tls->sem);
	free(tls);
}

typedef int (*stat_check_fn_t)(void *arg, const struct rf_stat *stat,
		const char *pcomp);
typedef int (*nid_stat_check_fn_t)(void *arg, const struct rf_stat *stat);

static struct udata *udata_unit_create_default(void)
{
	struct udata *udata;
	struct user *u;
	struct group *g;

	udata = udata_create_default();
	if (IS_ERR(udata))
		return udata;
	g = udata_add_group(udata, MSTORU_USERS_GROUP, MSTORU_USERS_GID);
	if (IS_ERR(g)) {
		udata_free(udata);
		return (struct udata*)g;
	}
	g = udata_add_group(udata, MSTORU_WOOTERS_GROUP,
			MSTORU_WOOTERS_GID);
	if (IS_ERR(g)) {
		udata_free(udata);
		return (struct udata*)g;
	}
	u = udata_add_user(udata, MSTORU_SPOONY_USER,
		MSTORU_SPOONY_UID, MSTORU_USERS_GID);
	if (IS_ERR(u)) {
		udata_free(udata);
		return (struct udata*)u;
	}
	u = udata_add_user(udata, MSTORU_WOOT_USER,
		MSTORU_WOOT_UID, MSTORU_USERS_GID);
	if (IS_ERR(u)) {
		udata_free(udata);
		return (struct udata*)u;
	}
	if (user_add_segid(udata, MSTORU_WOOT_USER, MSTORU_WOOTERS_GID)) {
		udata_free(udata);
		return ERR_PTR(-EINVAL);
	}
	return udata;
}

static int mstoru_set_primary_user_group(struct mstor *mstor, const char *user,
		const char *tgt_user, const char *tgt_group)
{
	struct mreq_add_user_to_group mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_SET_PRIMARY_USER_GROUP;
	mreq.base.user_name = user;
	mreq.tgt_user = tgt_user;
	mreq.tgt_group = tgt_group;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstoru_add_user_to_group(struct mstor *mstor, const char *user,
		const char *tgt_user, const char *tgt_group)
{
	struct mreq_add_user_to_group mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_ADD_USER_TO_GROUP;
	mreq.base.user_name = user;
	mreq.tgt_user = tgt_user;
	mreq.tgt_group = tgt_group;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstoru_remove_user_from_group(struct mstor *mstor, const char *user,
		const char *tgt_user, const char *tgt_group)
{
	struct mreq_add_user_to_group mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_REMOVE_USER_FROM_GROUP;
	mreq.base.user_name = user;
	mreq.tgt_user = tgt_user;
	mreq.tgt_group = tgt_group;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int test1_setup_users(struct mstor *mstor)
{
	/* The groups setup that we want:
	 *
	 * superuser: superuser
	 * spoony: spoony users
	 * wooters: wooters woot users
	 */
	EXPECT_EQ(mstoru_remove_user_from_group(mstor, MSTORU_SUPER_USER,
		MSTORU_SPOONY_USER, MSTORU_SPOONY_USER), -EINVAL);
	EXPECT_EQ(mstoru_remove_user_from_group(mstor, MSTORU_SUPER_USER,
		MSTORU_SPOONY_USER, MSTORU_USERS_GROUP), -ENOENT);
//	EXPECT_EQ(mstoru_remove_user_from_group(mstor, MSTORU_SPOONY_USER,
//		MSTORU_SPOONY_USER, MSTORU_USERS_GROUP), -EPERM);
	EXPECT_EQ(mstoru_add_user_to_group(mstor, MSTORU_SUPER_USER,
		MSTORU_SPOONY_USER, MSTORU_USERS_GROUP), 0);
//	EXPECT_EQ(mstoru_set_primary_user_group(mstor, MSTORU_SPOONY_USER,
//		MSTORU_WOOT_USER, MSTORU_USERS_GROUP), -EPERM);
	EXPECT_EQ(mstoru_set_primary_user_group(mstor, MSTORU_SUPER_USER,
		MSTORU_WOOT_USER, MSTORU_USERS_GROUP), 0);
	EXPECT_EQ(mstoru_set_primary_user_group(mstor, MSTORU_SUPER_USER,
		MSTORU_WOOT_USER, MSTORU_SPOONY_USER), 0);
	EXPECT_EQ(mstoru_add_user_to_group(mstor, MSTORU_SUPER_USER,
		MSTORU_WOOT_USER, MSTORU_WOOTERS_GROUP), 0);
	return 0;
}

static struct mstor *mstoru_init_unit(const char *tdir, const char *name,
		int cache_size, struct udata *udata)
{
	struct mstor *mstor;
	char mstor_path[PATH_MAX];
	struct mstorc *conf;

	if (zsnprintf(mstor_path, sizeof(mstor_path), "%s/%s", tdir, name))
		return ERR_PTR(ENAMETOOLONG);
	conf = JORM_INIT_mstorc();
	if (!conf)
		return ERR_PTR(ENOMEM);
	conf->mstor_path = strdup(mstor_path);
	if (!conf->mstor_path) {
		JORM_FREE_mstorc(conf);
		return ERR_PTR(ENOMEM);
	}
	conf->mstor_io_threads = MSTORU_NUM_IO_THREADS;
	conf->mstor_cache_mb = cache_size;
	mstor = mstor_init(g_fast_log_mgr, conf, udata);
	JORM_FREE_mstorc(conf);
	return mstor;
}

static int mstoru_test_open_close(const char *tdir)
{
	struct mstor *mstor;
	struct udata *udata;

	udata = udata_unit_create_default();
	EXPECT_NOT_ERRPTR(udata);
	mstor = mstoru_init_unit(tdir, "openclose", 1024, udata);
	EXPECT_NOT_ERRPTR(mstor);
	mstor_shutdown(mstor);
	udata_free(udata);
	return 0;
}

static int mstoru_do_mkdirs(struct mstor *mstor, const char *full_path,
		int mode, uint64_t ctime, const char *user_name)
{
	struct mreq_mkdirs mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_MKDIRS;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.mode = mode;
	mreq.ctime = ctime;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstoru_do_creat(struct mstor *mstor, const char *full_path,
		int mode, uint64_t ctime, const char *user_name, uint64_t *nid)
{
	int ret;
	struct mreq_creat mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_CREAT;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.mode = mode;
	mreq.ctime = ctime;
	ret = mstor_do_operation(mstor, (struct mreq*)&mreq);
	if (ret)
		return ret;
	*nid = mreq.nid;
	return 0;
}

static int mstoru_do_chunkalloc(struct mstor *mstor, uint64_t nid,
		uint64_t off, struct chunk_info *cinfo)
{
	int ret;
	struct mreq_chunkalloc mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_CHUNKALLOC;
	mreq.nid = nid;
	mreq.off = off;
	ret = mstor_do_operation(mstor, (struct mreq*)&mreq);
	if (ret)
		return ret;
	cinfo->cid = mreq.cid;
	cinfo->base = off;
	return 0;
}

static int mstoru_do_rename(struct mstor *mstor, const char *src,
		const char *dst, const char *user_name)
{
	struct mreq_rename mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_RENAME;
	mreq.base.full_path = src;
	mreq.base.user_name = user_name;
	mreq.dst_path = dst;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstoru_do_unlink(struct mstor *mstor, const char *full_path,
		const char *user_name, uint64_t ztime, enum mmm_unlink_op uop)
{
	struct mreq_unlink mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_UNLINK;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.ztime = ztime;
	mreq.uop = uop;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstoru_do_find_zombies(struct mstor *mstor,
		const struct zombie_info *lower_bound, int max_res,
		struct zombie_info *zinfos)
{
	int ret;
	struct mreq_find_zombies mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_FIND_ZOMBIES;
	mreq.lower_bound.cid = lower_bound->cid;
	mreq.lower_bound.ztime = lower_bound->ztime;
	mreq.max_res = max_res;
	mreq.zinfos = zinfos;
	ret = mstor_do_operation(mstor, (struct mreq*)&mreq);
	if (ret < 0)
		return ret;
	return mreq.num_res;
}

static int mstoru_do_destroy_zombie(struct mstor *mstor,
		const struct zombie_info *zinfo)
{
	struct mreq_destroy_zombie mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_DESTROY_ZOMBIE;
	mreq.zinfo.cid = zinfo->cid;
	mreq.zinfo.ztime = zinfo->ztime;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstoru_do_chunkfind(struct mstor *mstor, const char *full_path,
		uint64_t start, uint64_t end, const char *user_name,
		int max_cinfos, struct chunk_info *cinfos)
{
	int ret;
	struct mreq_chunkfind mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_CHUNKFIND;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.start = start;
	mreq.end = end;
	mreq.max_cinfos = max_cinfos;
	mreq.cinfos = cinfos;
	ret = mstor_do_operation(mstor, (struct mreq*)&mreq);
	if (ret)
		return FORCE_NEGATIVE(ret);
	return mreq.num_cinfos;
}

#define MSTORU_DO_LISTDIR_MAX_LE 128

static int mstoru_do_listdir(struct mstor *mstor, const char *full_path,
		const char *user_name, void *arg, stat_check_fn_t fn)
{
	int ret, i;
	struct mreq_listdir mreq;
	struct rf_lentry le_buf[MSTORU_DO_LISTDIR_MAX_LE];
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	memset(le_buf, 0, sizeof(le_buf));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_LISTDIR;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.le = le_buf;
	mreq.max_stat = MSTORU_DO_LISTDIR_MAX_LE;
	ret = mstor_do_operation(mstor, (struct mreq*)&mreq);
	if (ret) {
		fprintf(stderr, "do_listdir failed with error %d\n", ret);
		return FORCE_NEGATIVE(ret);
	}
	for (i = 0; i < mreq.num_stat; ++i) {
		ret = fn(arg, &mreq.le[i].stat, mreq.le[i].pcomp);
		if (ret)
			break;
	}
	for (i = 0; i < mreq.num_stat; ++i) {
		XDR_REQ_FREE(rf_lentry, &mreq.le[i]);
	}
	return (ret == 0) ? mreq.num_stat : FORCE_NEGATIVE(ret);
}

static int mstoru_do_utimes(struct mstor *mstor, const char *full_path,
		const char *user_name, uint64_t new_atime, uint64_t new_mtime)
{
	struct mreq_utimes mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_UTIMES;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.new_atime = new_atime;
	mreq.new_mtime = new_mtime;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstoru_do_stat(struct mstor *mstor, const char *full_path,
		const char *user_name, void *arg, stat_check_fn_t fn)
{
	int ret;
	struct rf_stat stat;
	struct mreq_stat mreq;
	struct mstoru_tls *tls = mstoru_tls_get();
	char pcomp[RF_PCOMP_MAX];

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_STAT;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.stat = &stat;
	ret = mstor_do_operation(mstor, (struct mreq*)&mreq);
	if (ret) {
		fprintf(stderr, "do_stat failed with error %d\n", ret);
		goto done;
	}
	if (fn) {
		ret = do_basename(pcomp, sizeof(pcomp), full_path);
		if (ret)
			goto done_free_resp;
		ret = fn(arg, mreq.stat, pcomp);
		if (ret)
			goto done_free_resp;
	}
	ret = 0;
done_free_resp:
	XDR_REQ_FREE(rf_stat, mreq.stat);
done:
	return FORCE_NEGATIVE(ret);
}

static int mstoru_do_nid_stat(struct mstor *mstor, uint64_t nid,
		void *arg, nid_stat_check_fn_t fn)
{
	int ret;
	struct rf_stat stat;
	struct mreq_nid_stat mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_NID_STAT;
	mreq.nid = nid;
	mreq.stat = &stat;
	ret = mstor_do_operation(mstor, (struct mreq*)&mreq);
	if (ret) {
		fprintf(stderr, "do_nid_stat failed with error %d\n", ret);
		goto done;
	}
	ret = fn(arg, mreq.stat);
	if (ret)
		goto done_free_resp;
	ret = 0;
done_free_resp:
	XDR_REQ_FREE(rf_stat, mreq.stat);
done:
	return FORCE_NEGATIVE(ret);
}

static int mstoru_do_chown(struct mstor *mstor, const char *full_path,
		const char *user_name, const char *new_user,
		const char *new_group)
{
	int ret;
	struct mreq_chown mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_CHOWN;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.new_user = new_user;
	mreq.new_group = new_group;
	ret = mstor_do_operation(mstor, (struct mreq*)&mreq);
	if (ret) {
		fprintf(stderr, "do_chown failed with error %d\n", ret);
		return FORCE_NEGATIVE(ret);
	}
	return 0;
}

static int mstoru_do_chmod(struct mstor *mstor, const char *full_path,
		const char *user_name, uint16_t mode)
{
	int ret;
	struct mreq_chmod mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_CHMOD;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.mode = mode;
	ret = mstor_do_operation(mstor, (struct mreq*)&mreq);
	if (ret) {
		fprintf(stderr, "do_chmod failed with error %d\n", ret);
		return FORCE_NEGATIVE(ret);
	}
	return 0;
}

struct mstoru_atime_and_mtime {
	uint64_t atime;
	uint64_t mtime;
};

static int test1_expect_c(void *arg,
		const struct rf_stat *stat, const char *pcomp)
{
	struct mstoru_atime_and_mtime *times = arg;
	EXPECT_EQ(stat->mode_and_type, MNODE_IS_DIR | 0644);
	EXPECT_EQ(stat->atime, times->atime);
	EXPECT_EQ(stat->mtime, times->mtime);
	//printf("pcomp='%s', uid='%d', gid='%d', "
	//	"user='%s', group='%s'\n", pcomp, stat->uid, stat->gid,
	//	stat->user, stat->group);
	EXPECT_ZERO(strcmp(pcomp, "c"));
	EXPECT_ZERO(strcmp(stat->user, MSTORU_SPOONY_USER));
	EXPECT_ZERO(strcmp(stat->group, MSTORU_USERS_GROUP));
	return 0;
}

static int test1_expect_root(void *arg,
		const struct rf_stat *stat, const char *pcomp)
{
	int expect_mode = (int)(uintptr_t)arg;

	EXPECT_EQ(stat->mode_and_type, expect_mode | MNODE_IS_DIR);
	//printf("pcomp='%s', uid='%s', gid='%d'\n", pcomp, uid, gid);
	EXPECT_ZERO(strcmp(pcomp, ""));
	EXPECT_ZERO(strcmp(stat->user, MSTORU_WOOT_USER));
	EXPECT_ZERO(strcmp(stat->group, MSTORU_USERS_GROUP));
	return 0;
}

static int test1_expect_root_nid(POSSIBLY_UNUSED(void *arg),
			     const struct rf_stat *stat)
{
	int mode = stat->mode_and_type & MMM_STAT_MODE_MASK;
	EXPECT_NONZERO(stat->mode_and_type & MMM_STAT_TYPE_DIR);
	EXPECT_EQ(mode, MSTOR_ROOT_NID_INIT_MODE);
	EXPECT_EQ(stat->nid, MSTOR_ROOT_NID);
	return 0;
}

static int mstoru_test1(const char *tdir)
{
	struct mstor *mstor;
	struct udata *udata;
	uint64_t nid;
	struct chunk_info cinfos1[MSTORU_MAX_CINFOS];
	struct chunk_info cinfos2[MSTORU_MAX_CINFOS];
	struct zombie_info lower_bound;
	struct zombie_info zinfos[MSTORU_MAX_ZINFOS];
	uint64_t csize = 134217728ULL;
	struct mstoru_atime_and_mtime times;

	udata = udata_unit_create_default();
	EXPECT_NOT_ERRPTR(udata);
	mstor = mstoru_init_unit(tdir, "test1", 1024, udata);
	EXPECT_NOT_ERRPTR(mstor);
	EXPECT_ZERO(test1_setup_users(mstor));
	EXPECT_EQ(mstoru_do_mkdirs(mstor, "/inval/user",
		0644, 123, MSTORU_INVALID_USER), -EUSERS);
	EXPECT_EQ(mstoru_do_nid_stat(mstor, MSTOR_ROOT_NID,
		NULL, test1_expect_root_nid), 0);
	/* change root node's mode to 0775 and group to 'users' */
	EXPECT_EQ(mstoru_do_chown(mstor, "/", RF_SUPERUSER_NAME,
		"", MSTORU_USERS_GROUP), 0);
	EXPECT_EQ(mstoru_do_chmod(mstor, "/", RF_SUPERUSER_NAME,
		0775), 0);
	EXPECT_ZERO(mstoru_do_creat(mstor, "/a", 0700, 123,
		MSTORU_SPOONY_USER, &nid));
	EXPECT_ZERO(mstoru_do_mkdirs(mstor, "/b/c",
		0644, 123, MSTORU_SPOONY_USER));
	EXPECT_EQ(mstoru_do_mkdirs(mstor, "/a/d/e",
		0644, 123, MSTORU_SPOONY_USER), -ENOTDIR);
	/* change mtime of /b/c */
	times.atime = 123ULL;
	times.mtime = 789ULL;
	EXPECT_EQ(mstoru_do_utimes(mstor, "/b/c", RF_SUPERUSER_NAME,
		RF_INVAL_TIME, times.mtime), 0);
	EXPECT_EQ(mstoru_do_stat(mstor, "/b/c", RF_SUPERUSER_NAME,
		&times, test1_expect_c), 0);
	//mstor_dump(mstor, stdout);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/a", MSTORU_SPOONY_USER,
		NULL, NULL), -ENOTDIR);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b", MSTORU_SPOONY_USER,
		&times, test1_expect_c), 1);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b/c", MSTORU_SPOONY_USER,
		NULL, NULL), -EPERM);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b/c", RF_SUPERUSER_NAME,
		NULL, NULL), 0);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b/c", RF_SUPERUSER_NAME,
		NULL, NULL), 0);
	EXPECT_EQ(mstoru_do_stat(mstor, "/b/c", RF_SUPERUSER_NAME,
		&times, test1_expect_c), 0);
	EXPECT_EQ(mstoru_do_chown(mstor, "/", RF_SUPERUSER_NAME,
		MSTORU_WOOT_USER, MSTORU_USERS_GROUP), 0);
	EXPECT_EQ(mstoru_do_chown(mstor, "/", MSTORU_SPOONY_USER,
		MSTORU_SPOONY_USER, ""), -EPERM);
	EXPECT_EQ(mstoru_do_stat(mstor, "/", MSTORU_SPOONY_USER,
		(void*)(uintptr_t)0775, test1_expect_root), 0);
	EXPECT_EQ(mstoru_do_chmod(mstor, "/b", RF_SUPERUSER_NAME,
		0770), 0);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b", MSTORU_SPOONY_USER,
		&times, test1_expect_c), 1);
	EXPECT_EQ(mstoru_do_chown(mstor, "/b", RF_SUPERUSER_NAME,
		RF_SUPERUSER_NAME, MSTORU_WOOTERS_GROUP), 0);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b", MSTORU_SPOONY_USER,
		NULL, NULL), -EPERM);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b", MSTORU_WOOT_USER,
		&times, test1_expect_c), 1);

	/* test rmdir */
	EXPECT_EQ(mstoru_do_unlink(mstor, "/", RF_SUPERUSER_NAME,
		456, MMM_UOP_RMDIR), -EINVAL);
	EXPECT_EQ(mstoru_do_unlink(mstor, "/b/c", RF_SUPERUSER_NAME,
		456, MMM_UOP_RMDIR), 0);
	EXPECT_ZERO(mstoru_do_mkdirs(mstor, "/b/c/m",
		0755, 123, MSTORU_WOOT_USER));
	EXPECT_EQ(mstoru_do_unlink(mstor, "/b/c", MSTORU_WOOT_USER,
		456, MMM_UOP_RMDIR), -ENOTEMPTY);
	EXPECT_EQ(mstoru_do_unlink(mstor, "/b/c", MSTORU_WOOT_USER,
		456, MMM_UOP_RMRF), 0);

	/* test file operations */
	EXPECT_ZERO(mstoru_do_mkdirs(mstor, "/b/c/d",
		0755, 123, MSTORU_WOOT_USER));
	EXPECT_EQ(mstoru_do_creat(mstor, "/b/c/d", 0700, 123,
		MSTORU_WOOT_USER, &nid), -EEXIST);
	EXPECT_EQ(mstoru_do_creat(mstor, "/b/c/d/foo", 0664, 123,
		MSTORU_SPOONY_USER, &nid), -EPERM);
	EXPECT_ZERO(mstoru_do_creat(mstor, "/b/c/d/foo", 0664, 123,
		MSTORU_WOOT_USER, &nid));

	EXPECT_ZERO(mstoru_do_chunkalloc(mstor, nid, 0, &cinfos1[0]));
	EXPECT_EQ(mstoru_do_chunkfind(mstor, "/b/c/d/foo", 0, 1,
		MSTORU_WOOT_USER, MSTORU_MAX_CINFOS, cinfos2), 1);
//	printf("cinfos1[0].cid = %"PRIx64", cinfos1[0].base = %"PRIx64"\n",
//	       cinfos[0].cid, cinfos[0].base);
	EXPECT_EQ(cinfos1[0].cid, cinfos2[0].cid);
	EXPECT_EQ(cinfos1[0].base, cinfos2[0].base);
	EXPECT_ZERO(mstoru_do_chunkalloc(mstor, nid,
			csize, &cinfos1[1]));
	EXPECT_ZERO(mstoru_do_chunkalloc(mstor, nid,
			csize * 2ULL, &cinfos1[2]));
	EXPECT_EQ(mstoru_do_chunkfind(mstor, "/b/c/d/foo", 0, csize * 10ULL,
		MSTORU_WOOT_USER, MSTORU_MAX_CINFOS, cinfos2), 3);
	EXPECT_EQ(cinfos1[1].cid, cinfos2[1].cid);
	EXPECT_EQ(cinfos1[1].base, cinfos2[1].base);
	EXPECT_EQ(cinfos1[2].cid, cinfos2[2].cid);
	EXPECT_EQ(cinfos1[2].base, cinfos2[2].base);
	EXPECT_ZERO(mstoru_do_creat(mstor, "/b/c/d/bar", 0664, 123,
		MSTORU_WOOT_USER, &nid));
	EXPECT_ZERO(mstoru_do_chunkalloc(mstor, nid, 0, &cinfos1[3]));

	/* test rename */
	EXPECT_EQ(mstoru_do_rename(mstor, "/b/c/d/foo", "/b/c/d/bar",
		MSTORU_WOOT_USER), -EEXIST);
	EXPECT_EQ(mstoru_do_rename(mstor, "/b/c/d/foo", "/b/c/d/bar",
		RF_SUPERUSER_NAME), -EEXIST);
	EXPECT_EQ(mstoru_do_rename(mstor, "/b/c/d/foo", "/b/c/d/foo2",
		MSTORU_INVALID_USER), -EUSERS);
	EXPECT_EQ(mstoru_do_rename(mstor, "/b/c/d/foo", "/b/c/d/foo",
		RF_SUPERUSER_NAME), 0);
	EXPECT_EQ(mstoru_do_rename(mstor, "/b/c/d/foo", "/b/c/d/foo2",
		MSTORU_WOOT_USER), 0);
	EXPECT_EQ(mstoru_do_rename(mstor, "/b/c/d/foo", "/b/c/d/foo2",
		MSTORU_WOOT_USER), -ENOENT);
	EXPECT_EQ(mstoru_do_rename(mstor, "/b/c/d/foo2", "/b/c/d/foo2",
		RF_SUPERUSER_NAME), 0);
	EXPECT_EQ(mstoru_do_rename(mstor, "/b/c/d/foo2", "/b/c/d/foo3",
		MSTORU_SPOONY_USER), -EPERM);
	EXPECT_EQ(mstoru_do_rename(mstor, "/b/c/d/foo2", "/b/c",
		RF_SUPERUSER_NAME), 0);
	EXPECT_EQ(mstoru_do_rename(mstor, "/b/c", "/b/c/d",
		RF_SUPERUSER_NAME), -EINVAL);
	EXPECT_EQ(mstoru_do_rename(mstor, "/", "/pizzaland",
		RF_SUPERUSER_NAME), -EINVAL);

	/* test unlink */
	EXPECT_EQ(mstoru_do_unlink(mstor, "/b/c/d",
		RF_SUPERUSER_NAME, 123, MMM_UOP_UNLINK), -EISDIR);
	EXPECT_EQ(mstoru_do_unlink(mstor, "/b/c/foo2",
		MSTORU_WOOT_USER, 124, MMM_UOP_UNLINK), 0);
	EXPECT_EQ(mstoru_do_unlink(mstor, "/b/c/d/bar",
		MSTORU_WOOT_USER, 125, MMM_UOP_UNLINK), 0);
	lower_bound.ztime = 123;
	lower_bound.cid = 0;
	EXPECT_EQ(mstoru_do_find_zombies(mstor, &lower_bound,
			MSTORU_MAX_ZINFOS, zinfos), 4);
	EXPECT_EQ(zinfos[0].cid, cinfos1[0].cid);
	EXPECT_EQ(zinfos[0].ztime, 124);
	EXPECT_EQ(zinfos[1].cid, cinfos1[1].cid);
	EXPECT_EQ(zinfos[1].ztime, 124);
	EXPECT_EQ(zinfos[2].cid, cinfos1[2].cid);
	EXPECT_EQ(zinfos[2].ztime, 124);
	EXPECT_EQ(zinfos[3].cid, cinfos1[3].cid);
	EXPECT_EQ(zinfos[3].ztime, 125);
	lower_bound.ztime = 127;
	lower_bound.cid = 0;
	EXPECT_EQ(mstoru_do_find_zombies(mstor, &lower_bound,
			MSTORU_MAX_ZINFOS, zinfos), 0);
	lower_bound.ztime = 125;
	lower_bound.cid = 0;
	EXPECT_EQ(mstoru_do_find_zombies(mstor, &lower_bound,
			MSTORU_MAX_ZINFOS, zinfos), 1);
	EXPECT_EQ(zinfos[0].cid, cinfos1[3].cid);
	EXPECT_EQ(zinfos[0].ztime, 125);
	EXPECT_ZERO(mstoru_do_destroy_zombie(mstor, &zinfos[0]));
	EXPECT_EQ(mstoru_do_find_zombies(mstor, &lower_bound,
			MSTORU_MAX_ZINFOS, zinfos), 0);

	/* test stat again */
	EXPECT_EQ(mstoru_do_chmod(mstor, "/", RF_SUPERUSER_NAME,
		0700), 0);
	EXPECT_EQ(mstoru_do_stat(mstor, "/", MSTORU_SPOONY_USER,
		(void*)(uintptr_t)0700, test1_expect_root), 0);

	mstor_shutdown(mstor);
	udata_free(udata);
	return 0;
}

struct mstoru_test2_tinfo {
	int tid;
	struct mstor *mstor;
};

static int do_mstoru_test2_impl(struct mstor *mstor)
{
	EXPECT_ZERO(mstoru_do_mkdirs(mstor, "/b/c/d",
		0755, 123, MSTORU_WOOT_USER));
	EXPECT_ZERO(mstoru_do_mkdirs(mstor, "/b/c/d/e",
		0755, 124, MSTORU_WOOT_USER));
	EXPECT_ZERO(mstoru_do_mkdirs(mstor, "/b/c/d/e/f",
		0755, 124, MSTORU_WOOT_USER));
	return 0;
}

static void* do_mstoru_test2(void *v)
{
	int ret;
	struct mstoru_test2_tinfo *tp = (struct mstoru_test2_tinfo*)v;

	ret = do_mstoru_test2_impl(tp->mstor);
	return (void*)(uintptr_t)FORCE_POSITIVE(ret);
}

static int mstoru_test2(const char *tdir)
{
	int i;
	struct mstor *mstor;
	struct udata *udata;
	pthread_t threads[MSTORU_NUM_IO_THREADS];
	struct mstoru_test2_tinfo tinfos[MSTORU_NUM_IO_THREADS];
	void *rval;

	udata = udata_unit_create_default();
	EXPECT_NOT_ERRPTR(udata);
	mstor = mstoru_init_unit(tdir, "test1", 1024, udata);
	EXPECT_NOT_ERRPTR(mstor);
	for (i = 0; i < MSTORU_NUM_IO_THREADS; ++i) {
		tinfos[i].tid = i;
		tinfos[i].mstor = mstor;
		EXPECT_ZERO(pthread_create(&threads[i], NULL, do_mstoru_test2,
			&tinfos[i]));
	}
	for (i = 0; i < MSTORU_NUM_IO_THREADS; ++i) {
		EXPECT_ZERO(pthread_join(threads[i], &rval));
		EXPECT_EQ(rval, NULL);
	}

	mstor_shutdown(mstor);
	udata_free(udata);
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	char tdir[PATH_MAX];

	EXPECT_ZERO(utility_ctx_init(argv[0])); /* for g_fast_log_mgr */
	EXPECT_ZERO(pthread_key_create(&g_tls_key, mstoru_tls_destroy));

	EXPECT_ZERO(get_tempdir(tdir, sizeof(tdir), 0755));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));
	EXPECT_ZERO(mstoru_test_open_close(tdir));
	EXPECT_ZERO(mstoru_test1(tdir));
	EXPECT_ZERO(mstoru_test2(tdir));

	EXPECT_ZERO(pthread_key_delete(g_tls_key));
	process_ctx_shutdown();

	return EXIT_SUCCESS;
}
