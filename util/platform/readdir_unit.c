/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/dir.h"
#include "util/platform/readdir.h"
#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECTED_MAX 100

static int test_do_readdir_impl(const char *dir, const char **expected)
{
	const char **e;
	char err[512] = { 0 };
	struct redfish_dirp *dp;
	int num_expected = 0, num_found = 0;

	do_mkdir(dir, 0775, err, sizeof(err));
	EXPECT_ZERO(err[0]);
	for (e = expected; *e; ++e) {
		do_touch1(*e);
		++num_expected;
	}
	EXPECT_ZERO(do_opendir(dir, &dp));
	while (1) {
		struct dirent *de;
		de = do_readdir(dp);
		if (!de)
			break;
		if (strcmp(de->d_name, "."))
			continue;
		else if (strcmp(de->d_name, ".."))
			continue;
		for (e = expected; *e; ++e) {
			if (strcmp(de->d_name, *e) == 0) {
				++num_found;
				break;
			}
		}
		if (*e == NULL) {
			fprintf(stderr, "found unknown directory entry '%s'\n",
				de->d_name);
			return -EIO;
		}
	}
	EXPECT_EQUAL(num_found, num_expected);
	do_closedir(dp);
	return 0;
}

static int test_do_readdir(const char *tdir, const char **input)
{
	int i = 0;
	const char **in;
	char expected[EXPECTED_MAX][PATH_MAX];
	const char *oexpected[EXPECTED_MAX + 1] = { 0 };

	memset(expected, 0, sizeof(expected));
	for (in = input; *in; ++in) {
		EXPECT_LT(i, EXPECTED_MAX);
		EXPECT_ZERO(zsnprintf(&expected[i][0], PATH_MAX, "%s/%s",
					tdir, input[i]));
		++i;
	}
	for (; i > 0; --i) {
		oexpected[i] = expected[0];
	}
	return test_do_readdir_impl(tdir, oexpected);
}

int main(void)
{
	char tempdir[PATH_MAX] = { 0 };
	const char *in1[] = { "foo", "bar", "baz", NULL };
	const char *in2[] = { NULL };
	const char *in3[] = { "one.file", NULL };
	const char *in4[] = { "huge_______________________filename", "blah",
		"blah2", "blah3", "blah4", "blah56", NULL };

	EXPECT_ZERO(get_tempdir(tempdir, PATH_MAX, 0775));
	EXPECT_ZERO(register_tempdir_for_cleanup(tempdir));

	EXPECT_ZERO(test_do_readdir(tempdir, in1));
	EXPECT_ZERO(test_do_readdir(tempdir, in2));
	EXPECT_ZERO(test_do_readdir(tempdir, in3));
	EXPECT_ZERO(test_do_readdir(tempdir, in4));

	return EXIT_SUCCESS;
}
