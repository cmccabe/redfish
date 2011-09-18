/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/tempfile.h"
#include "util/test.h"

#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int test_get_tempdir(void)
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

int main(void)
{
	int ret;
	ret = test_get_tempdir();
	if (ret)
		return ret;
	ret = test_get_tempdir();
	if (ret)
		return ret;
	ret = test_get_tempdir();
	if (ret)
		return ret;
	return EXIT_SUCCESS;
}
