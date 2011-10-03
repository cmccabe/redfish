/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "osd/io.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SZ (32 * 1024 * 1024)

int main(void)
{
	int ret;
	char *buf, *buf2;
	char tempdir[PATH_MAX];
	ret = get_tempdir(tempdir, PATH_MAX, 0770);
	if (ret)
		return EXIT_FAILURE;
	ret = register_tempdir_for_cleanup(tempdir);
	if (ret)
		return EXIT_FAILURE;
	ret = onechunk_set_prefix(tempdir);
	if (ret)
		return EXIT_FAILURE;

	fprintf(stderr, "allocating buffer...\n");
	buf = malloc(BUF_SZ);
	if (!buf)
		return EXIT_FAILURE;
	fprintf(stderr, "allocated buffer...\n");
	memset(buf, 'c', BUF_SZ);
	ret = onechunk_write(0, buf, BUF_SZ, 0);
	if (ret)
		return EXIT_FAILURE;

	buf2 = calloc(1, BUF_SZ);
	if (!buf2)
		return EXIT_FAILURE;
	ret = onechunk_read(0, buf2, BUF_SZ, 0);
	if (ret < 0)
		return EXIT_FAILURE;
	if (ret != BUF_SZ) {
		fprintf(stderr, "read back not enough data\n");
		return EXIT_FAILURE;
	}
	if (memcmp(buf, buf2, BUF_SZ)) {
		fprintf(stderr, "read back different data\n");
		return EXIT_FAILURE;
	}
	free(buf);
	free(buf2);
	return EXIT_SUCCESS;
}
