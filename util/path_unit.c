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

#include "util/path.h"
#include "util/string.h"
#include "util/test.h"

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
	}
	else {
		EXPECT_ZERO(canonicalize_path(epath));
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

int main(void)
{
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

	return EXIT_SUCCESS;
}
