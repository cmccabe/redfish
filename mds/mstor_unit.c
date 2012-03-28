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
#include "mds/limits.h"
#include "mds/mstor.h"
#include "mds/srange_lock.h"
#include "mds/user.h"
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

typedef int (*stat_check_fn_t)(void *arg, struct mmm_packed_stat *hdr,
		const char *pcomp, const char *owner, const char *group);

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
		const char *user_name, uint64_t ztime)
{
	struct mreq_unlink mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_UNLINK;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.ztime = ztime;
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

static int mstoru_do_listdir(struct mstor *mstor, const char *full_path,
		const char *user_name, void *arg, stat_check_fn_t fn)
{
	int ret, num_entries;
	struct mmm_packed_stat *hdr;
	struct mreq_listdir mreq;
	char buf[16384], pcomp[RF_PCOMP_MAX];
	char owner[RF_USER_MAX], group[RF_GROUP_MAX];
	uint32_t off;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	memset(buf, 0, sizeof(buf));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_LISTDIR;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.out = buf;
	mreq.out_len = sizeof(buf);
	ret = mstor_do_operation(mstor, (struct mreq*)&mreq);
	if (ret) {
		fprintf(stderr, "do_listdir failed with error %d\n", ret);
		return FORCE_NEGATIVE(ret);
	}
	off = 0;
	num_entries = 0;
	while (1) {
		if (off >= mreq.used_len)
			break;
		ret = unpack_str(buf, &off, mreq.used_len, pcomp,
				RF_PCOMP_MAX);
		if (ret)
			return FORCE_NEGATIVE(ret);
		if (off + sizeof(struct mmm_packed_stat) >= mreq.used_len)
			return -EIO;
		hdr = (struct mmm_packed_stat*)(buf + off);
		off += sizeof(struct mmm_packed_stat);
		ret = unpack_str(buf, &off, mreq.used_len, owner,
				RF_USER_MAX);
		if (ret)
			return FORCE_NEGATIVE(ret);
		ret = unpack_str(buf, &off, mreq.used_len, group,
				RF_GROUP_MAX);
		if (ret)
			return FORCE_NEGATIVE(ret);
		if (fn) {
			ret = fn(arg, hdr, pcomp, owner, group);
			if (ret)
				return FORCE_NEGATIVE(ret);
		}
		num_entries++;
	}
	//fprintf(stderr, "mreq.used_len = %Zd\n", mreq.used_len);
	return num_entries;
}

static int mstoru_do_rmdir(struct mstor *mstor, const char *full_path,
		const char *user_name, uint64_t ztime, int rmr)
{
	struct mreq_rmdir mreq;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_RMDIR;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.ztime = ztime;
	mreq.rmr = rmr;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstoru_do_stat(struct mstor *mstor, const char *full_path,
		const char *user_name, void *arg, stat_check_fn_t fn)
{
	int ret;
	struct mmm_packed_stat *hdr;
	struct mreq_stat mreq;
	char buf[16384];
	char owner[RF_USER_MAX], group[RF_GROUP_MAX], pcomp[RF_PCOMP_MAX];
	uint32_t off;
	struct mstoru_tls *tls = mstoru_tls_get();

	memset(&mreq, 0, sizeof(mreq));
	memset(buf, 0, sizeof(buf));
	mreq.base.lk = tls->lk;
	mreq.base.op = MSTOR_OP_STAT;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.out = buf;
	mreq.out_len = sizeof(buf);
	ret = mstor_do_operation(mstor, (struct mreq*)&mreq);
	if (ret) {
		fprintf(stderr, "do_stat failed with error %d\n", ret);
		return FORCE_NEGATIVE(ret);
	}
	hdr = (struct mmm_packed_stat*)buf;
	off = offsetof(struct mmm_packed_stat, data);
	ret = unpack_str(hdr, &off, mreq.out_len, owner, RF_USER_MAX);
	if (ret)
		return FORCE_NEGATIVE(ret);
	ret = unpack_str(buf, &off, mreq.out_len, group, RF_GROUP_MAX);
	if (ret)
		return FORCE_NEGATIVE(ret);
	ret = do_basename(pcomp, sizeof(pcomp), full_path);
	if (ret)
		return FORCE_NEGATIVE(ret);
	if (fn) {
		ret = fn(arg, hdr, pcomp, owner, group);
		if (ret)
			return FORCE_NEGATIVE(ret);
	}
	return 0;
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

static int test1_expect_c(POSSIBLY_UNUSED(void *arg),
		struct mmm_packed_stat *hdr, const char *pcomp,
		const char *owner, const char *group)
{
	EXPECT_EQ(unpack_from_be16(&hdr->mode_and_type),
			MNODE_IS_DIR | 0644);
	EXPECT_EQ(unpack_from_be64(&hdr->mtime), 123ULL);
	EXPECT_EQ(unpack_from_be64(&hdr->atime), 123ULL);
	//printf("pcomp='%s', uid='%d', gid='%d'\n", pcomp, uid, gid);
	EXPECT_ZERO(strcmp(pcomp, "c"));
	printf("owner = '%s'\n", owner);
	EXPECT_ZERO(strcmp(owner, MSTORU_SPOONY_USER));
	printf("group = '%s'\n", group);
	EXPECT_ZERO(strcmp(group, MSTORU_USERS_GROUP));
	return 0;
}

static int test1_expect_root(void *arg, struct mmm_packed_stat *hdr,
		const char *pcomp, const char *owner, const char *group)
{
	int expect_mode = (int)(uintptr_t)arg;

	EXPECT_EQ(unpack_from_be16(&hdr->mode_and_type),
			expect_mode | MNODE_IS_DIR);
	//printf("pcomp='%s', uid='%s', gid='%d'\n", pcomp, uid, gid);
	EXPECT_ZERO(strcmp(pcomp, ""));
	EXPECT_ZERO(strcmp(owner, MSTORU_WOOT_USER));
	EXPECT_ZERO(strcmp(group, MSTORU_USERS_GROUP));
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

	udata = udata_unit_create_default();
	EXPECT_NOT_ERRPTR(udata);
	mstor = mstoru_init_unit(tdir, "test1", 1024, udata);
	EXPECT_NOT_ERRPTR(mstor);
	EXPECT_EQ(mstoru_do_mkdirs(mstor, "/inval/user",
		0644, 123, MSTORU_INVALID_USER), -EUSERS);
	/* change root node's mode to 0775 and group to 'users' */
	EXPECT_EQ(mstoru_do_chown(mstor, "/", RF_SUPERUSER_NAME,
		NULL, MSTORU_USERS_GROUP), 0);
	EXPECT_EQ(mstoru_do_chmod(mstor, "/", RF_SUPERUSER_NAME,
		0775), 0);
	EXPECT_ZERO(mstoru_do_creat(mstor, "/a", 0700, 123,
		MSTORU_SPOONY_USER, &nid));
	EXPECT_ZERO(mstoru_do_mkdirs(mstor, "/b/c",
		0644, 123, MSTORU_SPOONY_USER));
	EXPECT_EQ(mstoru_do_mkdirs(mstor, "/a/d/e",
		0644, 123, MSTORU_SPOONY_USER), -ENOTDIR);
	//mstor_dump(mstor, stdout);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/a", MSTORU_SPOONY_USER,
		NULL, NULL), -ENOTDIR);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b", MSTORU_SPOONY_USER,
		NULL, test1_expect_c), 1);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b/c", MSTORU_SPOONY_USER,
		NULL, NULL), -EPERM);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b/c", RF_SUPERUSER_NAME,
		NULL, NULL), 0);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b/c", RF_SUPERUSER_NAME,
		NULL, NULL), 0);
	EXPECT_EQ(mstoru_do_stat(mstor, "/b/c", RF_SUPERUSER_NAME,
		NULL, test1_expect_c), 0);
	EXPECT_EQ(mstoru_do_chown(mstor, "/", RF_SUPERUSER_NAME,
		MSTORU_WOOT_USER, MSTORU_USERS_GROUP), 0);
	EXPECT_EQ(mstoru_do_chown(mstor, "/", MSTORU_SPOONY_USER,
		MSTORU_SPOONY_USER, NULL), -EPERM);
	EXPECT_EQ(mstoru_do_stat(mstor, "/", MSTORU_SPOONY_USER,
		(void*)(uintptr_t)0775, test1_expect_root), 0);
	EXPECT_EQ(mstoru_do_chmod(mstor, "/b", RF_SUPERUSER_NAME,
		0770), 0);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b", MSTORU_SPOONY_USER,
		NULL, test1_expect_c), 1);
	EXPECT_EQ(mstoru_do_chown(mstor, "/b", RF_SUPERUSER_NAME,
		RF_SUPERUSER_NAME, MSTORU_WOOTERS_GROUP), 0);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b", MSTORU_SPOONY_USER,
		NULL, NULL), -EPERM);
	EXPECT_EQ(mstoru_do_listdir(mstor, "/b", MSTORU_WOOT_USER,
		NULL, test1_expect_c), 1);

	/* test rmdir */
	EXPECT_EQ(mstoru_do_rmdir(mstor, "/", RF_SUPERUSER_NAME,
		456, 0), -EINVAL);
	EXPECT_EQ(mstoru_do_rmdir(mstor, "/b/c", RF_SUPERUSER_NAME,
		456, 0), 0);
	EXPECT_ZERO(mstoru_do_mkdirs(mstor, "/b/c/m",
		0755, 123, MSTORU_WOOT_USER));
	EXPECT_EQ(mstoru_do_rmdir(mstor, "/b/c", MSTORU_WOOT_USER,
		456, 0), -ENOTEMPTY);
	EXPECT_EQ(mstoru_do_rmdir(mstor, "/b/c", MSTORU_WOOT_USER,
		456, 1), 0);

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
		RF_SUPERUSER_NAME, 123), -EISDIR);
	EXPECT_EQ(mstoru_do_unlink(mstor, "/b/c/foo2",
		MSTORU_WOOT_USER, 124), 0);
	EXPECT_EQ(mstoru_do_unlink(mstor, "/b/c/d/bar",
		MSTORU_WOOT_USER, 125), 0);
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
