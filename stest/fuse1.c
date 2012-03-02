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

#include "core/env.h"
#include "stest/stest.h"
#include "util/error.h"
#include "util/platform/readdir.h"
#include "util/string.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

static const char *g_fuse1_test_usage[] = {
	"fuse1_test: tests doing some things with our FUSE mountpoint.",
	"test-specific environment variables:",
	"REDFISH_FUSE_DIR: set the FUSE mountpoint we should test.",
	NULL
};

static int expect_a_b(const struct dirent *de, POSSIBLY_UNUSED(void *data))
{
	if (!strcmp(de->d_name, "a"))
		return 0;
	if (!strcmp(de->d_name, "b"))
		return 0;
	return -EINVAL;
}

static int fuse1_test(const char *fdir)
{
	struct redfish_dirp *dp;
	char path[PATH_MAX], src[PATH_MAX], dst[PATH_MAX];
	struct stat st_buf;
	int ret;
	struct utimbuf tbuf;

	/* stat the fuse mount point */
	ST_EXPECT_ZERO(stat(fdir, &st_buf));
	ST_EXPECT_NONZERO(S_ISDIR(st_buf.st_mode));

	/* mkdir test */
	snprintf(path, sizeof(path), "%s/dir1", fdir);
	RETRY_ON_EINTR_GET_ERRNO(ret, mkdir(path, 0755));
	ST_EXPECT_ZERO(ret);

	/* rmdir test */
	RETRY_ON_EINTR_GET_ERRNO(ret, rmdir(path));
	ST_EXPECT_ZERO(ret);

	/* mkdir test2 */
	ST_EXPECT_ZERO(zsnprintf(path, sizeof(path), "%s/dir2", fdir));
	RETRY_ON_EINTR_GET_ERRNO(ret, mkdir(path, 0755));
	ST_EXPECT_ZERO(ret);
	ST_EXPECT_ZERO(zsnprintf(path, sizeof(path), "%s/dir2/a", fdir));
	RETRY_ON_EINTR_GET_ERRNO(ret, mkdir(path, 0755));
	ST_EXPECT_ZERO(ret);
	ST_EXPECT_ZERO(zsnprintf(path, sizeof(path), "%s/dir2/b", fdir));
	RETRY_ON_EINTR_GET_ERRNO(ret, mkdir(path, 0755));
	ST_EXPECT_ZERO(ret);

	/* listdir test */
	ST_EXPECT_ZERO(zsnprintf(path, sizeof(path), "%s/dir2", fdir));
	ST_EXPECT_ZERO(do_opendir(path, &dp));
	ST_EXPECT_EQ(stest_fuse_readdir(dp, expect_a_b, NULL), 2);
	do_closedir(dp);

	/* rmdir test2 */
	ST_EXPECT_ZERO(zsnprintf(path, sizeof(path), "%s/dir2/a", fdir));
	RETRY_ON_EINTR_GET_ERRNO(ret, rmdir(path));
	ST_EXPECT_ZERO(ret);

	/* listdir test2 */
	ST_EXPECT_ZERO(zsnprintf(path, sizeof(path), "%s/dir2", fdir));
	ST_EXPECT_ZERO(do_opendir(path, &dp));
	ST_EXPECT_EQ(stest_fuse_readdir(dp, expect_a_b, NULL), 1);
	do_closedir(dp);

	/* unlink test */
	ST_EXPECT_ZERO(zsnprintf(path, sizeof(path), "%s/dir2/b", fdir));
	RETRY_ON_EINTR_GET_ERRNO(ret, unlink(path));
	ST_EXPECT_EQ(ret, -EISDIR);

	/* rename test */
	ST_EXPECT_ZERO(zsnprintf(src, sizeof(src), "%s/dir2/b", fdir));
	ST_EXPECT_ZERO(zsnprintf(dst, sizeof(dst), "%s/dir2/a", fdir));
	ST_EXPECT_ZERO(rename(src, dst));
	RETRY_ON_EINTR_GET_ERRNO(ret, rename(src, dst));
	ST_EXPECT_EQ(ret, -ENOENT);

	/* utime test */
	ST_EXPECT_ZERO(zsnprintf(path, sizeof(path), "%s/dir2/a", fdir));
	memset(&tbuf, 0, sizeof(tbuf));
	tbuf.actime = 123;
	tbuf.modtime = 456;
	ST_EXPECT_ZERO(utime(path, &tbuf));
	return 0;
}

static void fuse1_test_cleanup(const char *fdir)
{
	int ret;
	size_t i;
	const char *dir;
	const char *dirs[] = { "dir1", "dir2" };
	char cmd[PATH_MAX];

	for (i = 0; i < sizeof(dirs)/sizeof(dirs[0]); ++i) {
		dir = dirs[i];
		if (zsnprintf(cmd, sizeof(cmd), "rm -rf '%s/%s'\n",
				fdir, dir)) {
			stest_add_error("failed to remove %s: -ENAMETOOLONG\n",
					dir);
			return;
		}
		ret = system(cmd);
		if (ret < 0) {
			stest_add_error("failed to remove %s: system(3) "
					"failed.\n", dir);
			return;
		}
		ret = WEXITSTATUS(ret);
		if (ret != 0) {
			stest_add_error("failed to remove %s: system(3) "
					"returned exit status %d\n", dir, ret);
			return;
		}
	}
}

static int path_is_dir(const char *path)
{
	struct stat st_buf;

	if (stat(path, &st_buf) < 0) {
		return -errno;
	}
	if (S_ISDIR(st_buf.st_mode))
		return 1;
	return 0;
}

int main(int argc, char **argv)
{
	const char *fdir;

	fdir = getenv_or_die("REDFISH_FUSE_DIR");
	if (path_is_dir(fdir) != 1) {
		stest_add_error("REDFISH_FUSE_DIR does not point to "
			"a directory.\n");
		return EXIT_FAILURE;
	}
	stest_init(argc, argv, g_fuse1_test_usage);
	fuse1_test_cleanup(fdir);
	if (fuse1_test(fdir))
		goto done;
	fuse1_test_cleanup(fdir);
done:
	return stest_finish();
}
