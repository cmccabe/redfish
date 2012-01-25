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

#include "client/stub/xattrs.h"
#include "util/error.h"
#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define TEST_FNAME1 "xattrs.test.1"

#define TEST_KEY1 "user.key1"
#define TEST_VAL1 "val1"

#define TEST_KEY2 "user.key2"
#define TEST_BASE2 8
#define TEST_VAL2 0755

#define TEST_KEY3 "user.key3"
#define TEST_BASE3 10
#define TEST_VAL3 5636

#define TEST_KEY4 "user.key4"
#define TEST_BASE4 16
#define TEST_VAL4 0xfff

#define TEST_KEY5 "user.key5"
#define TEST_VAL5 "cheese!"

int main(void)
{
	int fd, ret, i;
	char *x, tdir[PATH_MAX] = { 0 }, tfile[PATH_MAX];

	EXPECT_ZERO(get_tempdir(tdir, PATH_MAX, 0700));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));

	ret = check_xattr_support(tdir);
	if (ret) {
		fprintf(stderr, "check_xattr_support: got %d.\n"
			"Can't run this unit test because the filesystem at "
			"'%s' does not support xattrs.\n", ret, tdir);
		return EXIT_SUCCESS;
	}

	/* file doesn't exist... should get error */
	EXPECT_ZERO(zsnprintf(tfile, sizeof(tfile), "%s/%s",
			      tdir, TEST_FNAME1));
	EXPECT_NONZERO(xsets(tfile, TEST_KEY1, TEST_VAL1));

	/* create file */
	EXPECT_ZERO(do_touch1(tfile));

	/* set xattr string */
	EXPECT_ZERO(xsets(tfile, TEST_KEY1, TEST_VAL1));
	EXPECT_ZERO(xgets(tfile, TEST_KEY1, PATH_MAX, &x));
	EXPECT_ZERO(strcmp(x, TEST_VAL1));
	free(x);

	/* set base-8 xattr int */
	EXPECT_ZERO(xseti(tfile, TEST_KEY2, TEST_BASE2, TEST_VAL2));
	EXPECT_ZERO(xgeti(tfile, TEST_KEY2, TEST_BASE2, &i));
	EXPECT_EQ(i, TEST_VAL2);

	/* set base-10 xattr int */
	EXPECT_ZERO(xseti(tfile, TEST_KEY3, TEST_BASE3, TEST_VAL3));
	EXPECT_ZERO(xgeti(tfile, TEST_KEY3, TEST_BASE3, &i));
	EXPECT_EQ(i, TEST_VAL3);

	/* set base-16 xattr int */
	fd = open(tfile, O_RDONLY);
	EXPECT_GT(fd, 0);
	EXPECT_ZERO(fxseti(fd, TEST_KEY4, TEST_BASE4, TEST_VAL4));
	EXPECT_ZERO(fxgeti(fd, TEST_KEY4, TEST_BASE4, &i));
	EXPECT_EQ(i, TEST_VAL4);

	/* set another xattr string */
	EXPECT_ZERO(fxsets(fd, TEST_KEY5, TEST_VAL5));
	EXPECT_ZERO(fxgets(fd, TEST_KEY5, PATH_MAX, &x));
	EXPECT_ZERO(strcmp(x, TEST_VAL5));
	free(x);
	RETRY_ON_EINTR(ret, close(fd));

	return EXIT_SUCCESS;
}
