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

#include "util/dir.h"
#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_do_mkdir(const char *tempdir, const char *dir_name)
{
	char path[PATH_MAX], err[512] = { 0 };
	if (zsnprintf(path, PATH_MAX, "%s/%s", tempdir, dir_name)) {
		fprintf(stderr, "path too long!\n");
		return 1;
	}
	do_mkdir(path, 0775, err, sizeof(err));
	if (err[0])
		return 1;
	else
		return 0;
}

static int test_do_mkdir_p(const char *tempdir, const char *dir_name)
{
	char path[PATH_MAX];
	if (zsnprintf(path, PATH_MAX, "%s/%s", tempdir, dir_name)) {
		fprintf(stderr, "path too long!\n");
		return 1;
	}
	return do_mkdir_p(path, 0775);
}

int main(void)
{
	char tempdir[PATH_MAX] = { 0 };

	EXPECT_ZERO(get_tempdir(tempdir, PATH_MAX, 0775));
	EXPECT_ZERO(register_tempdir_for_cleanup(tempdir));

	EXPECT_ZERO(test_do_mkdir(tempdir, "foo"));
	EXPECT_ZERO(test_do_mkdir(tempdir, "bar"));
	EXPECT_ZERO(test_do_mkdir(tempdir, "bar"));
	EXPECT_ZERO(do_touch2(tempdir, "a_file"));
	EXPECT_NONZERO(test_do_mkdir(tempdir, "a_file"));
	EXPECT_NONZERO(test_do_mkdir_p(tempdir, "a_file"));
	EXPECT_ZERO(test_do_mkdir_p(tempdir, "a_dir/a_dir2/a_dir3"));
	EXPECT_ZERO(test_do_mkdir_p(tempdir, "blah/blah/blah/blah"));
	EXPECT_ZERO(test_do_mkdir_p(tempdir, "blah/blah/blah/blah"));
	EXPECT_ZERO(test_do_mkdir_p(tempdir, "blah/blah/blah/blah2/"));
	EXPECT_ZERO(do_touch2(tempdir, "blah/blah/blah/blah2/blah3"));
	EXPECT_NONZERO(test_do_mkdir_p(tempdir,
			"blah/blah/blah/blah2/blah3/blah4"));
	EXPECT_ZERO(test_do_mkdir_p(tempdir, "blah2"));

	return EXIT_SUCCESS;
}
