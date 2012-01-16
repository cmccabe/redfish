/*
 * Copyright 2011-2012 the RedFish authors
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
#include "mds/user.h"
#include "msg/generic.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/packed.h"
#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MSU_SPOONY_USER "spoony"
#define MSU_SPOONY_UID 2

#define MSU_WOOT_USER "woot"
#define MSU_WOOT_UID 3

#define MSU_UNIT_USERS_GROUP "users"
#define MSU_UNIT_USERS_GID 2

#define MSU_UNIT_WOOTERS_GROUP "wooters"
#define MSU_UNIT_WOOTERS_GID 3

typedef int (*stat_check_fn_t)(void *arg, struct mmm_stat_hdr *hdr,
		const char *pcomp);

static struct udata *udata_unit_create_default(void)
{
	struct udata *udata;
	struct user *u;
	struct group *g;

	udata = udata_create_default();
	if (IS_ERR(udata))
		return udata;
	g = udata_add_group(udata, MSU_UNIT_USERS_GROUP, MSU_UNIT_USERS_GID);
	if (IS_ERR(g)) {
		udata_free(udata);
		return (struct udata*)g;
	}
	g = udata_add_group(udata, MSU_UNIT_WOOTERS_GROUP,
			MSU_UNIT_WOOTERS_GID);
	if (IS_ERR(g)) {
		udata_free(udata);
		return (struct udata*)g;
	}
	u = udata_add_user(udata, MSU_SPOONY_USER,
		MSU_SPOONY_UID, MSU_UNIT_USERS_GID);
	if (IS_ERR(u)) {
		udata_free(udata);
		return (struct udata*)u;
	}
	u = udata_add_user(udata, MSU_WOOT_USER,
		MSU_WOOT_UID, MSU_UNIT_USERS_GID);
	if (IS_ERR(u)) {
		udata_free(udata);
		return (struct udata*)u;
	}
	if (user_add_segid(udata, MSU_WOOT_USER, MSU_UNIT_WOOTERS_GID)) {
		udata_free(udata);
		return ERR_PTR(-EINVAL);
	}
	return udata;
}

static struct mstor *mstor_init_unit(const char *tdir, const char *name,
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
	conf->mstor_cache_size = cache_size;
	mstor = mstor_init(g_fast_log_mgr, conf, udata);
	JORM_FREE_mstorc(conf);
	return mstor;
}

static int mstor_test_open_close(const char *tdir)
{
	struct mstor *mstor;
	struct udata *udata;

	udata = udata_unit_create_default();
	EXPECT_NOT_ERRPTR(udata);
	mstor = mstor_init_unit(tdir, "openclose", 1024, udata);
	EXPECT_NOT_ERRPTR(mstor);
	mstor_shutdown(mstor);
	udata_free(udata);
	return 0;
}

static int mstor_do_mkdirs(struct mstor *mstor, const char *full_path,
		int mode, uint64_t ctime, const char *user_name)
{
	struct mreq_mkdirs mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.op = MSTOR_OP_MKDIRS;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.mode = mode;
	mreq.ctime = ctime;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstor_do_creat(struct mstor *mstor, const char *full_path,
		int mode, uint64_t ctime, const char *user_name, uint64_t *nid)
{
	int ret;
	struct mreq_creat mreq;

	memset(&mreq, 0, sizeof(mreq));
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

static int unpack_stat_hdr(struct mmm_stat_hdr *hdr, char *pcomp)
{
	int ret;
	uint16_t stat_len;
	uint32_t off;

	stat_len = unpack_from_be16(&hdr->stat_len);
	off = offsetof(struct mmm_stat_hdr, data);
	ret = unpack_str(hdr, &off, stat_len, pcomp, RF_PCOMP_MAX);
	if (ret)
		return ret;
	return 0;
}

static int mstor_do_listdir(struct mstor *mstor, const char *full_path,
		const char *user_name, void *arg, stat_check_fn_t fn)
{
	int ret, num_entries;
	struct mmm_stat_hdr *hdr;
	struct mreq_listdir mreq;
	char buf[16384];
	char pcomp[RF_PCOMP_MAX];
	uint32_t off;

	memset(&mreq, 0, sizeof(mreq));
	memset(buf, 0, sizeof(buf));
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
		hdr = (struct mmm_stat_hdr*)(buf + off);
		ret = unpack_stat_hdr(hdr, pcomp);
		if (ret) {
			return FORCE_NEGATIVE(ret);
		}
		if (fn)
			ret = fn(arg, hdr, pcomp);
		if (ret) {
			return FORCE_NEGATIVE(ret);
		}
		off += unpack_from_be16(&hdr->stat_len);
		num_entries++;
	}
	//fprintf(stderr, "mreq.used_len = %Zd\n", mreq.used_len);
	return num_entries;
}

static int mstor_do_rmdir(struct mstor *mstor, const char *full_path,
		const char *user_name, uint64_t ztime, int rmr)
{
	struct mreq_rmdir mreq;
	mreq.base.op = MSTOR_OP_RMDIR;
	mreq.base.full_path = full_path;
	mreq.base.user_name = user_name;
	mreq.ztime = ztime;
	mreq.rmr = rmr;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstor_do_stat(struct mstor *mstor, const char *full_path,
		const char *user_name, void *arg, stat_check_fn_t fn)
{
	int ret;
	struct mmm_stat_hdr *hdr;
	struct mreq_stat mreq;
	char buf[16384];
	char pcomp[RF_PCOMP_MAX];
	uint32_t off;

	memset(&mreq, 0, sizeof(mreq));
	memset(buf, 0, sizeof(buf));
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
	off = 0;
	hdr = (struct mmm_stat_hdr*)buf;
	ret = unpack_stat_hdr(hdr, pcomp);
	if (ret)
		return FORCE_NEGATIVE(ret);
	if (fn)
		ret = fn(arg, hdr, pcomp);
	if (ret)
		return FORCE_NEGATIVE(ret);
	return 0;
}

static int mstor_do_chown(struct mstor *mstor, const char *full_path,
		const char *user_name, const char *new_user,
		const char *new_group)
{
	int ret;
	struct mreq_chown mreq;

	memset(&mreq, 0, sizeof(mreq));
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

static int mstor_do_chmod(struct mstor *mstor, const char *full_path,
		const char *user_name, uint16_t mode)
{
	int ret;
	struct mreq_chmod mreq;

	memset(&mreq, 0, sizeof(mreq));
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
		struct mmm_stat_hdr *hdr, const char *pcomp)
{
	EXPECT_EQUAL(unpack_from_be16(&hdr->mode_and_type),
			MNODE_IS_DIR | 0644);
	EXPECT_EQUAL(unpack_from_be64(&hdr->mtime), 123ULL);
	EXPECT_EQUAL(unpack_from_be64(&hdr->atime), 123ULL);
	//printf("pcomp='%s', uid='%d', gid='%d'\n", pcomp, uid, gid);
	EXPECT_ZERO(strcmp(pcomp, "c"));
	EXPECT_EQUAL(unpack_from_be32(&hdr->uid), MSU_SPOONY_UID);
	EXPECT_EQUAL(unpack_from_be32(&hdr->gid), MSU_UNIT_USERS_GID);
	return 0;
}

static int test1_expect_root(void *arg,
		struct mmm_stat_hdr *hdr, const char *pcomp)
{
	int expect_mode = (int)(uintptr_t)arg;

	EXPECT_EQUAL(unpack_from_be16(&hdr->mode_and_type),
			expect_mode | MNODE_IS_DIR);
	//printf("pcomp='%s', uid='%s', gid='%d'\n", pcomp, uid, gid);
	EXPECT_ZERO(strcmp(pcomp, ""));
	EXPECT_EQUAL(unpack_from_be32(&hdr->uid), MSU_WOOT_UID);
	EXPECT_EQUAL(unpack_from_be32(&hdr->gid), MSU_UNIT_USERS_GID);
	return 0;
}

static int mstor_test1(const char *tdir)
{
	struct mstor *mstor;
	struct udata *udata;
	uint64_t nid;

	udata = udata_unit_create_default();
	EXPECT_NOT_ERRPTR(udata);
	mstor = mstor_init_unit(tdir, "test1", 1024, udata);
	EXPECT_NOT_ERRPTR(mstor);
	/* change root node's mode to 0775 and group to 'users' */
	EXPECT_EQUAL(mstor_do_chown(mstor, "/", RF_SUPERUSER_NAME,
		NULL, MSU_UNIT_USERS_GROUP), 0);
	EXPECT_EQUAL(mstor_do_chmod(mstor, "/", RF_SUPERUSER_NAME,
		0775), 0);
	EXPECT_ZERO(mstor_do_creat(mstor, "/a", 0700, 123,
		MSU_SPOONY_USER, &nid));
	EXPECT_ZERO(mstor_do_mkdirs(mstor, "/b/c",
		0644, 123, MSU_SPOONY_USER));
	EXPECT_EQUAL(mstor_do_mkdirs(mstor, "/a/d/e",
		0644, 123, MSU_SPOONY_USER), -ENOTDIR);
	//mstor_dump(mstor, stdout);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/a", MSU_SPOONY_USER,
		NULL, NULL), -ENOTDIR);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b", MSU_SPOONY_USER,
		NULL, test1_expect_c), 1);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b/c", MSU_SPOONY_USER,
		NULL, NULL), -EPERM);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b/c", RF_SUPERUSER_NAME,
		NULL, NULL), 0);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b/c", RF_SUPERUSER_NAME,
		NULL, NULL), 0);
	EXPECT_EQUAL(mstor_do_stat(mstor, "/b/c", RF_SUPERUSER_NAME,
		NULL, test1_expect_c), 0);
	EXPECT_EQUAL(mstor_do_chown(mstor, "/", RF_SUPERUSER_NAME,
		MSU_WOOT_USER, MSU_UNIT_USERS_GROUP), 0);
	EXPECT_EQUAL(mstor_do_chown(mstor, "/", MSU_SPOONY_USER,
		MSU_SPOONY_USER, NULL), -EPERM);
	EXPECT_EQUAL(mstor_do_stat(mstor, "/", MSU_SPOONY_USER,
		(void*)(uintptr_t)0775, test1_expect_root), 0);
	EXPECT_EQUAL(mstor_do_chmod(mstor, "/b", RF_SUPERUSER_NAME,
		0770), 0);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b", MSU_SPOONY_USER,
		NULL, test1_expect_c), 1);
	EXPECT_EQUAL(mstor_do_chown(mstor, "/b", RF_SUPERUSER_NAME,
		RF_SUPERUSER_NAME, MSU_UNIT_WOOTERS_GROUP), 0);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b", MSU_SPOONY_USER,
		NULL, NULL), -EPERM);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b", MSU_WOOT_USER,
		NULL, test1_expect_c), 1);

	/* test rmdir */
	EXPECT_EQUAL(mstor_do_rmdir(mstor, "/", RF_SUPERUSER_NAME,
		456, 0), -EINVAL);
	EXPECT_EQUAL(mstor_do_rmdir(mstor, "/b/c", RF_SUPERUSER_NAME,
		456, 0), 0);
	EXPECT_ZERO(mstor_do_mkdirs(mstor, "/b/c/m",
		0755, 123, MSU_WOOT_USER));
	EXPECT_EQUAL(mstor_do_rmdir(mstor, "/b/c", MSU_WOOT_USER,
		456, 0), -ENOTEMPTY);
	EXPECT_EQUAL(mstor_do_rmdir(mstor, "/b/c", MSU_WOOT_USER,
		456, 1), 0);

	/* test file operations */
	EXPECT_ZERO(mstor_do_mkdirs(mstor, "/b/c/d",
		0755, 123, MSU_WOOT_USER));
	EXPECT_EQUAL(mstor_do_creat(mstor, "/b/c/d", 0700, 123,
		MSU_WOOT_USER, &nid), -EEXIST);
	EXPECT_EQUAL(mstor_do_creat(mstor, "/b/c/d/foo", 0664, 123,
		MSU_SPOONY_USER, &nid), -EPERM);
	EXPECT_ZERO(mstor_do_creat(mstor, "/b/c/d/foo", 0664, 123,
		MSU_WOOT_USER, &nid));

	EXPECT_EQUAL(mstor_do_chmod(mstor, "/", RF_SUPERUSER_NAME,
		0700), 0);
	EXPECT_EQUAL(mstor_do_stat(mstor, "/", MSU_SPOONY_USER,
		(void*)(uintptr_t)0700, test1_expect_root), 0);

	mstor_shutdown(mstor);
	udata_free(udata);
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	char tdir[PATH_MAX];

	EXPECT_ZERO(utility_ctx_init(argv[0])); /* for g_fast_log_mgr */

	EXPECT_ZERO(get_tempdir(tdir, sizeof(tdir), 0755));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));
	EXPECT_ZERO(mstor_test_open_close(tdir));
	EXPECT_ZERO(mstor_test1(tdir));

	process_ctx_shutdown();

	return EXIT_SUCCESS;
}
