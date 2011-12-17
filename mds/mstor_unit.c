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

typedef int (*stat_check_fn_t)(void *arg, struct mmm_stat_hdr *hdr,
		const char *pcomp, const char *owner, const char *group);

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
		uint64_t mtime)
{
	struct mreq_creat mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.op = MSTOR_OP_CREAT;
	mreq.base.full_path = full_path;
	mreq.base.user = "spoony";
	mreq.base.group = "spoony";
	mreq.mode = 0644;
	mreq.mtime = mtime;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstor_do_mkdirs(struct mstor *mstor, const char *full_path,
		int mode, uint64_t mtime)
{
	struct mreq_mkdirs mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.op = MSTOR_OP_MKDIRS;
	mreq.base.full_path = full_path;
	mreq.base.user = "spoony";
	mreq.base.group = "spoony";
	mreq.mode = mode;
	mreq.mtime = mtime;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int unpack_stat_hdr(struct mmm_stat_hdr *hdr,
		char *pcomp, char *owner, char *group)
{
	int ret;
	uint16_t stat_len;
	uint32_t off;

	stat_len = unpack_from_be16(&hdr->stat_len);
	off = offsetof(struct mmm_stat_hdr, data);
	ret = unpack_str(hdr, &off, stat_len, pcomp, RF_PCOMP_MAX);
	if (ret)
		return ret;
	//printf("WW unpacked pcomp as '%s'\n", pcomp);
	ret = unpack_str(hdr, &off, stat_len, owner, RF_USER_MAX);
	if (ret)
		return ret;
	//printf("WW unpacked owner as '%s'\n", owner);
	ret = unpack_str(hdr, &off, stat_len, group, RF_GROUP_MAX);
	if (ret)
		return ret;
	//printf("WW unpacked group as '%s'\n", group);
	return 0;
}

static int mstor_do_listdir(struct mstor *mstor, const char *full_path,
		const char *muser, const char *mgroup,
		void *arg, stat_check_fn_t fn)
{
	int ret, num_entries;
	struct mmm_stat_hdr *hdr;
	struct mreq_listdir mreq;
	char buf[16384];
	char pcomp[RF_PCOMP_MAX], owner[RF_USER_MAX], group[RF_GROUP_MAX];
	uint32_t off;

	memset(&mreq, 0, sizeof(mreq));
	memset(buf, 0, sizeof(buf));
	mreq.base.op = MSTOR_OP_LISTDIR;
	mreq.base.full_path = full_path;
	mreq.base.user = muser;
	mreq.base.group = mgroup;
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
		ret = unpack_stat_hdr(hdr, pcomp, owner, group);
		if (ret) {
			return FORCE_NEGATIVE(ret);
		}
		if (fn)
			ret = fn(arg, hdr, pcomp, owner, group);
		if (ret) {
			return FORCE_NEGATIVE(ret);
		}
		off += unpack_from_be16(&hdr->stat_len);
		num_entries++;
	}
	//fprintf(stderr, "mreq.used_len = %Zd\n", mreq.used_len);
	return num_entries;
}

static int test1_expect_c(POSSIBLY_UNUSED(void *arg),
		struct mmm_stat_hdr *hdr, const char *pcomp, const char *owner,
		const char *group)
{
	EXPECT_EQUAL(unpack_from_be16(&hdr->mode_and_type),
			MNODE_IS_DIR | 0644);
	EXPECT_EQUAL(unpack_from_be64(&hdr->mtime), 123ULL);
	EXPECT_EQUAL(unpack_from_be64(&hdr->atime), 123ULL);
	EXPECT_ZERO(strcmp(pcomp, "c"));
	EXPECT_ZERO(strcmp(owner, "spoony"));
	EXPECT_ZERO(strcmp(group, "spoony"));
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
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/a", "spoony", "spoony",
			NULL, NULL), -ENOTDIR);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b", "spoony", "spoony",
			NULL, test1_expect_c), 1);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b/c", "spoony", "spoony",
			NULL, NULL), -EPERM);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b/c", "superuser", "spoony",
			NULL, NULL), 0);
	EXPECT_EQUAL(mstor_do_listdir(mstor, "/b/c", "spoony", "superuser",
			NULL, NULL), 0);
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
