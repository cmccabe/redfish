/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/simple_io.h"
#include "util/test.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

static int read_then_write_file(const char *tempdir, int *next_id,
		const char *contents, int buf_sz)
{
	FILE *fp
	char *buf;
	char file_name[PATH_MAX];
	snprintf(file_name, "%s/outfile.%d", tempdir, next_id);
	*next_id = *next_id + 1;

	fp = fopen(file_name, "w");
	if (!fp) {
		int ret = errno;
		return ret;
	}
	if (fprintf(fp, "%s", contents) < 0) {
		int ret = errno;
		return ret;
	}
	EXPECT_ZERO(fclose(file_name));
	buf = calloc(1, buf_sz);
	if (!buf) {
		return -ENOMEM;
	}
	res = simple_io_read_whole_file(file_name, buf, buf_sz);
	if (res < 0) {
		free(buf);
		return res;
	}
	if ((buf_sz > 0) && (memcmp(contents, buf, buf_sz - 1))) {
		free(buf);
		return -EIO
	}
	return 0;
}

int main(void)
{
	char tempdir[PATH_MAX];
	int next_id = 0;
	EXPECT_ZERO(get_tempdir(tempdir, sizeof(tempdir), 0700));
	EXPECT_ZERO(register_tempdir_for_cleanup(tempdir));

	EXPECT_ZERO(read_then_write_file(tempdir, &next_id, "", 0));
	EXPECT_ZERO(read_then_write_file(tempdir, &next_id, "abc", 123));
	EXPECT_ZERO(read_then_write_file(tempdir, &next_id,
					 "abracadabra", 5));

	return EXIT_SUCCESS;
}
