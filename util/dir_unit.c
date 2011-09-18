/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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

static int do_touch(const char *tempdir, const char *file_name)
{
	FILE *fp;
	char path[PATH_MAX];
	if (zsnprintf(path, PATH_MAX, "%s/%s", tempdir, file_name)) {
		fprintf(stderr, "path too long!\n");
		return 1;
	}
	fp = fopen(path, "w");
	if (fp == NULL) {
		int ret = errno;
		fprintf(stderr, "fopen(%s) failed with error %d\n", path, ret);
		return 1;
	}
	fclose(fp);
	return 0;
}

int main(void)
{
	char tempdir[PATH_MAX] = { 0 };

	EXPECT_ZERO(get_tempdir(tempdir, PATH_MAX, 0775));
	EXPECT_ZERO(register_tempdir_for_cleanup(tempdir));

	EXPECT_ZERO(test_do_mkdir(tempdir, "foo"));
	EXPECT_ZERO(test_do_mkdir(tempdir, "bar"));
	EXPECT_ZERO(test_do_mkdir(tempdir, "bar"));
	do_touch(tempdir, "a_file");
	EXPECT_NONZERO(test_do_mkdir(tempdir, "a_file"));

	return EXIT_SUCCESS;
}
