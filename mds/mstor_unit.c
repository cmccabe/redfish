/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "common/config/mstorc.h"
#include "core/process_ctx.h"
#include "mds/limits.h"
#include "mds/mstor.h"
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

#define MSU_SPOONY_UID 2
#define MSU_SPOONY_GID 2
#define MSU_WOOT_UID 3
#define MSU_WOOT_GID 3

typedef int (*stat_check_fn_t)(void *arg, struct mmm_stat_hdr *hdr,
		const char *pcomp);

static struct mstor *mstor_init_unit(const char *tdir, const char *name,
		int cache_size)
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
	mstor = mstor_init(g_fast_log_mgr, conf);
	JORM_FREE_mstorc(conf);
	return mstor;
}

static int mstor_test_open_close(const char *tdir)
{
	struct mstor *mstor;

	mstor = mstor_init_unit(tdir, "openclose", 1024);
	EXPECT_NOT_ERRPTR(mstor);
	mstor_shutdown(mstor);
	return 0;
}

static int mstor_do_creat(struct mstor *mstor, const char *full_path,
		uint64_t ctime)
{
	struct mreq_creat mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.op = MSTOR_OP_CREAT;
	mreq.base.full_path = full_path;
	mreq.base.uid = MSU_SPOONY_UID;
	mreq.base.gid = MSU_SPOONY_GID;
	mreq.mode = 0644;
	mreq.ctime = ctime;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstor_do_mkdirs(struct mstor *mstor, const char *full_path,
		int mode, uint64_t ctime)
{
	struct mreq_mkdirs mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.op = MSTOR_OP_MKDIRS;
	mreq.base.full_path = full_path;
	mreq.base.uid = MSU_SPOONY_UID;
	mreq.base.gid = MSU_SPOONY_GID;
	mreq.mode = mode;
	mreq.ctime = ctime;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
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
		uint32_t uid, uint32_t gid, void *arg, stat_check_fn_t fn)
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
	mreq.base.uid = uid;
	mreq.base.gid = gid;
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

static int mstor_do_stat(struct mstor *mstor, const char *full_path,
		uint32_t uid, uint32_t gid, void *arg, stat_check_fn_t fn)
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
	mreq.base.uid = uid;
	mreq.base.gid = gid;
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
	uint32_t uid, uint32_t gid, uint32_t new_uid, uint32_t new_gid)
{
	int ret;
	struct mreq_chown mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.op = MSTOR_OP_CHOWN;
	mreq.base.full_path = full_path;
	mreq.base.uid = uid;
	mreq.base.gid = gid;
	mreq.new_uid = new_uid;
	mreq.new_gid = new_gid;
	ret = mstor_do_operation(mstor, (struct mreq*)&mreq);
	if (ret) {
		fprintf(stderr, "do_chown failed with error %d\n", ret);
		return FORCE_NEGATIVE(ret);
	}
	return 0;
}

static int mstor_do_chmod(struct mstor *mstor, const char *full_path,
		uint32_t uid, uint32_t gid, uint16_t mode)
{
	int ret;
	struct mreq_chmod mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.op = MSTOR_OP_CHMOD;
	mreq.base.full_path = full_path;
	mreq.base.uid = uid;
	mreq.base.gid = gid;
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
	EXPECT_EQUAL(unpack_from_be32(&hdr->gid), MSU_SPOONY_GID);
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
	EXPECT_EQUAL(unpack_from_be32(&hdr->gid), MSU_WOOT_GID);
	return 0;
}

static int mstor_test1(const char *tdir)
{
	struct mstor *mstor;

	mstor = mstor_init_unit(tdir, "test1", 1024);
	EXPECT_NOT_ERRPTR(mstor);
	EXPECT_ZERO(mstor_do_creat(mstor, "/a", 123));
	EXPECT_ZERO(mstor_do_mkdirs(mstor, "/b/c", 0644, 123));
	EXPECT_EQUAL(mstor_do_mkdirs(mstor, "/a/d/e", 0644, 123), -ENOTDIR);

	//mstor_dump(mstor, stdout);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/a", MSU_SPOONY_UID,
		MSU_SPOONY_GID, NULL, NULL), -ENOTDIR);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b", MSU_SPOONY_UID,
		MSU_SPOONY_GID, NULL, test1_expect_c), 1);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b/c", MSU_SPOONY_UID,
		MSU_SPOONY_GID, NULL, NULL), -EPERM);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b/c", RF_SUPERUSER_UID,
		MSU_SPOONY_GID, NULL, NULL), 0);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b/c", RF_SUPERUSER_UID,
		RF_SUPERUSER_GID, NULL, NULL), 0);
	EXPECT_EQUAL(mstor_do_stat(mstor, "/b/c", RF_SUPERUSER_UID,
		MSU_SPOONY_GID, NULL, test1_expect_c), 0);
	EXPECT_EQUAL(mstor_do_chown(mstor, "/", RF_SUPERUSER_UID,
		RF_SUPERUSER_GID, MSU_WOOT_UID, MSU_WOOT_GID), 0);
//	EXPECT_EQUAL(mstor_do_chown(mstor, "/", MSU_WOOT_UID,
//		MSU_WOOT_GID, MSU_WOOT_UID, MSU_GID_WOOT), -EPERM);
	EXPECT_EQUAL(mstor_do_stat(mstor, "/", MSU_SPOONY_UID, MSU_SPOONY_GID,
			(void*)(uintptr_t)0755, test1_expect_root), 0);
	EXPECT_EQUAL(mstor_do_chmod(mstor, "/", MSU_SPOONY_UID, MSU_SPOONY_GID,
			0700), 0);
	EXPECT_EQUAL(mstor_do_stat(mstor, "/", MSU_SPOONY_UID, MSU_SPOONY_GID,
			(void*)(uintptr_t)0700, test1_expect_root), 0);
	mstor_shutdown(mstor);
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
