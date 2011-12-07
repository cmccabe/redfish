/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "common/config/mstorc.h"
#include "core/process_ctx.h"
#include "mds/mstor.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
	conf->mstor_path = mstor_path;
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
	mreq.base.user = "superuser";
	mreq.base.group = "superuser";
	mreq.mode = 0644;
	mreq.mtime = mtime;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstor_do_mkdirs(struct mstor *mstor, const char *full_path,
		uint64_t mtime)
{
	struct mreq_mkdirs mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.base.op = MSTOR_OP_MKDIRS;
	mreq.base.full_path = full_path;
	mreq.base.user = "superuser";
	mreq.base.group = "superuser";
	mreq.mode = 0644;
	mreq.mtime = mtime;
	return mstor_do_operation(mstor, (struct mreq*)&mreq);
}

static int mstor_test1(const char *tdir)
{
	struct mstor *mstor;

	mstor = mstor_init_unit(tdir, "test1", 1024);
	EXPECT_NOT_ERRPTR(mstor);
	EXPECT_NONZERO(mstor_do_creat(mstor, "/a", 123));
	EXPECT_NONZERO(mstor_do_mkdirs(mstor, "/b/c", 123));
	EXPECT_EQUAL(mstor_do_mkdirs(mstor, "/a/d/e", 123), -ENOTDIR);
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
