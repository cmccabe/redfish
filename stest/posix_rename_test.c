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

#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* A simple test of what POSIX rename does in a bunch of different
 * scenarios.  This doesn't test certain tricky corner cases like a rename
 * where both src and dst are a hardlink to the same file.  It's just your
 * basic directories and files.
 */
static int do_rename(const char *src_base, const char *src_suffix,
		     const char *dst_base, const char *dst_suffix)
{
	char src[PATH_MAX], dst[PATH_MAX];

	EXPECT_ZERO(zsnprintf(src, PATH_MAX, "%s/%s", src_base, src_suffix));
	EXPECT_ZERO(zsnprintf(dst, PATH_MAX, "%s/%s", dst_base, dst_suffix));
	if (rename(src, dst) < 0) {
		int err = -errno;
		return err;
	}
	return 0;
}

static int do_test_mkdir(const char *base, const char *suffix)
{
	char path[PATH_MAX];

	EXPECT_ZERO(zsnprintf(path, PATH_MAX, "%s/%s", base, suffix));
	if (mkdir(path, 0755) < 0) {
		return -errno;
	}
	return 0;
}

int main(void)
{
	char dir_src[PATH_MAX];
	char dir_dst[PATH_MAX];

	EXPECT_ZERO(get_tempdir(dir_src, PATH_MAX, 0755));
	EXPECT_ZERO(register_tempdir_for_cleanup(dir_src));
	EXPECT_ZERO(get_tempdir(dir_dst, PATH_MAX, 0755));
	EXPECT_ZERO(register_tempdir_for_cleanup(dir_dst));

	/* test rename /src/a -> /dst/m, a as file, m as file */
	EXPECT_ZERO(do_touch2(dir_src, "a"));
	EXPECT_ZERO(do_touch2(dir_dst, "m"));
	EXPECT_ZERO(do_rename(dir_src, "a", dir_dst, "m"));

	/* test rename /src/b -> /dst/n, b as file, n as dir */
	EXPECT_ZERO(do_touch2(dir_src, "b"));
	EXPECT_ZERO(do_test_mkdir(dir_dst, "n"));
	EXPECT_EQ(do_rename(dir_src, "b", dir_dst, "n"), -EISDIR);

	/* test rename /src/c -> /dst/o, c as dir, o as file */
	EXPECT_ZERO(do_test_mkdir(dir_src, "c"));
	EXPECT_ZERO(do_touch2(dir_dst, "o"));
	EXPECT_EQ(do_rename(dir_src, "c", dir_dst, "o"), -ENOTDIR);

	/* test rename /src/d -> /dst/p, d as dir, p as dir */
	EXPECT_ZERO(do_test_mkdir(dir_src, "d"));
	EXPECT_ZERO(do_test_mkdir(dir_dst, "p"));
	EXPECT_ZERO(do_rename(dir_src, "d", dir_dst, "p"));

	/* test rename /src/e -> /dst/q, e as file, q as nonexistent */
	EXPECT_ZERO(do_touch2(dir_src, "e"));
	EXPECT_ZERO(do_rename(dir_src, "e", dir_dst, "q"));

	/* test rename /src/f -> /dst/r, f as dir, r as nonexistent */
	EXPECT_ZERO(do_test_mkdir(dir_src, "f"));
	EXPECT_ZERO(do_rename(dir_src, "f", dir_dst, "r"));

	return 0;
}
