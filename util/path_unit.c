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

#include "mds/limits.h"
#include "msg/types.h"
#include "util/path.h"
#include "util/string.h"
#include "util/test.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_canonicalize_path(const char *path, const char *expected)
{
	char epath[PATH_MAX];

	EXPECT_ZERO(zsnprintf(epath, PATH_MAX, "%s", path));
	if (!expected) {
		EXPECT_NONZERO(canonicalize_path(epath));
		EXPECT_LT(canonicalize_path2(epath, sizeof(epath), path), 0);
	}
	else {
		EXPECT_ZERO(canonicalize_path(epath));
		EXPECT_ZERO(strcmp(epath, expected));
		EXPECT_GT(canonicalize_path2(epath, sizeof(epath), path), 0);
		EXPECT_ZERO(strcmp(epath, expected));
	}
	return 0;
}

static int test_do_dirname(const char *fname, const char *expected)
{
	char dname[PATH_MAX] = { 0 };

	do_dirname(fname, dname, PATH_MAX);
	return strcmp(dname, expected);
}

static int test_do_basename(const char *fname, const char *expected)
{
	char bname[PATH_MAX] = { 0 };

	EXPECT_ZERO(do_basename(bname, PATH_MAX, fname));
	return strcmp(bname, expected);
}

static int test_canon_path_append(void)
{
	char base1[RF_PATH_MAX] = "/";
	char base2[RF_PATH_MAX] = "/a/b/c";
	char base3[RF_PATH_MAX] = "/foo";

	EXPECT_ZERO(canon_path_append(base1, sizeof(base1), "foo"));
	EXPECT_ZERO(strcmp(base1, "/foo"));
	EXPECT_ZERO(canon_path_append(base2, sizeof(base2), "d/e/f"));
	EXPECT_ZERO(strcmp(base2, "/a/b/c/d/e/f"));
	EXPECT_ZERO(canon_path_append(base3, sizeof(base3), ""));
	EXPECT_ZERO(strcmp(base3, "/foo"));
	return 0;
}

static int test_canon_path_add_suffix(void)
{
	char path1[RF_PATH_MAX] = "/";
	char path2[RF_PATH_MAX] = "/a";

	EXPECT_ZERO(canon_path_add_suffix(path1, sizeof(path1), '/'));
	EXPECT_ZERO(strcmp(path1, "/"));
	EXPECT_ZERO(canon_path_add_suffix(path1, sizeof(path1), '0'));
	EXPECT_ZERO(strcmp(path1, "0"));
	EXPECT_ZERO(canon_path_add_suffix(path2, sizeof(path2), '/'));
	EXPECT_ZERO(strcmp(path2, "/a/"));
	strcpy(path2, "/a");
	EXPECT_ZERO(canon_path_add_suffix(path2, sizeof(path2), '0'));
	EXPECT_ZERO(strcmp(path2, "/a0"));

	return 0;
}

int main(void)
{
	char buf[4];

	EXPECT_ZERO(test_canonicalize_path("", NULL));
	EXPECT_ZERO(test_canonicalize_path("./foo", NULL));
	EXPECT_ZERO(test_canonicalize_path("/tmp//foo", "/tmp/foo"));
	EXPECT_ZERO(test_canonicalize_path("////tmp/////foo/", "/tmp/foo"));
	EXPECT_ZERO(test_canonicalize_path("/tmp/foo", "/tmp/foo"));
	EXPECT_ZERO(test_canonicalize_path("/long/path/that/ends/in/a/slash/",
			       "/long/path/that/ends/in/a/slash"));
	EXPECT_ZERO(test_canonicalize_path("/", "/"));

	EXPECT_ZERO(test_do_dirname("/", "/"));
	EXPECT_ZERO(test_do_dirname("/tmp/foo", "/tmp"));
	EXPECT_ZERO(test_do_dirname("/longer/path/here", "/longer/path"));
	EXPECT_ZERO(test_do_basename("/", ""));
	EXPECT_ZERO(test_do_basename("/tmp/foo", "foo"));
	EXPECT_ZERO(test_do_basename("/longer/path/here", "here"));
	EXPECT_ZERO(test_canon_path_append());
	EXPECT_ZERO(test_canon_path_add_suffix());

	EXPECT_EQ(canonicalize_path2(buf, sizeof(buf), "/a"), 2);
	EXPECT_EQ(canonicalize_path2(buf, sizeof(buf), "/a/b/c"),
		-ENAMETOOLONG);

	return EXIT_SUCCESS;
}
