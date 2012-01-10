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

#include "util/tempfile.h"
#include "util/test.h"

#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int test_create_tempdir(void)
{
	char tempdir[PATH_MAX];
	struct stat st_buf;
	int ret;

	ret = get_tempdir(tempdir, PATH_MAX, 0770);
	if (ret)
		return EXIT_FAILURE;
	ret = register_tempdir_for_cleanup(tempdir);
	if (ret)
		return EXIT_FAILURE;
	if (stat(tempdir, &st_buf) == -1) {
		return EXIT_FAILURE;
	}
	if (!S_ISDIR(st_buf.st_mode)) {
		return EXIT_FAILURE;
	}
	return 0;
}

static int test_create_tempdir_and_delete(void)
{
	char tempdir[PATH_MAX];
	struct stat st_buf;
	int ret;

	ret = get_tempdir(tempdir, PATH_MAX, 0775);
	if (ret)
		return EXIT_FAILURE;
	ret = register_tempdir_for_cleanup(tempdir);
	if (ret)
		return EXIT_FAILURE;
	if (stat(tempdir, &st_buf) == -1) {
		return EXIT_FAILURE;
	}
	if (!S_ISDIR(st_buf.st_mode)) {
		return EXIT_FAILURE;
	}
	remove_tempdir(tempdir);
	unregister_tempdir_for_cleanup(tempdir);
	return 0;
}

int main(void)
{
	EXPECT_ZERO(test_create_tempdir());
	EXPECT_ZERO(test_create_tempdir());
	EXPECT_ZERO(test_create_tempdir());
	EXPECT_ZERO(test_create_tempdir_and_delete());
	EXPECT_ZERO(test_create_tempdir_and_delete());
	EXPECT_ZERO(test_create_tempdir_and_delete());
	return EXIT_SUCCESS;
}
