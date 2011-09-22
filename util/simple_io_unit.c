/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/simple_io.h"
#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BIGBUF_SZ 16384

static int read_then_write_file(const char *tempdir, int *next_id,
		const char *contents, int buf_sz)
{
	ssize_t res;
	FILE *fp;
	char *buf, file_name[PATH_MAX];
	EXPECT_ZERO(zsnprintf(file_name, PATH_MAX,"%s/outfile.%d",
			      tempdir, *next_id));
	*next_id = *next_id + 1;

	fp = fopen(file_name, "w");
	if (!fp) {
		int ret = errno;
		fprintf(stderr, "failed to open '%s': error %d\n",
			file_name, ret);
		return ret;
	}
	if (fprintf(fp, "%s", contents) < 0) {
		int ret = errno;
		fprintf(stderr, "failed to write to '%s': error %d\n",
			file_name, ret);
		return ret;
	}
	EXPECT_ZERO(fclose(fp));
	buf = calloc(1, buf_sz);
	if (!buf) {
		return -ENOMEM;
	}
	res = simple_io_read_whole_file(file_name, buf, buf_sz);
	if (res < 0) {
		free(buf);
		fprintf(stderr, "simple_io_read_whole_file failed with "
			"error %Zd\n", res);
		return res;
	}
	if ((buf_sz > 0) && (strncmp(contents, buf, buf_sz - 1))) {
		fprintf(stderr, "got contents: '%s'; expected first "
			"%d characters of: '%s' \n",
			buf, buf_sz - 1, contents);
		free(buf);
		return -EIO;
	}
	free(buf);
	return 0;
}

static int test_copy_fd_to_fd(const char *tempdir, int *next_id,
				const char *buf)
{
	size_t buf_len = strlen(buf);
	ssize_t res;
	char *nbuf, src_file[PATH_MAX], dst_file[PATH_MAX];
	FILE *ifp, *ofp;
	int ret;
	EXPECT_ZERO(zsnprintf(src_file, sizeof(src_file),
		      "%s/src_file.%d", tempdir, *next_id));
	EXPECT_ZERO(zsnprintf(dst_file, sizeof(dst_file),
		      "%s/dst_file.%d", tempdir, *next_id));
	*next_id = *next_id + 1;
	ifp = fopen(src_file, "w");
	if (!ifp) {
		ret = errno; 
		return ret;
	}
	EXPECT_EQUAL(fwrite(buf, 1, buf_len, ifp), buf_len);
	ifp = freopen(src_file, "r", ifp);
	if (!ifp) {
		ret = errno; 
		return ret;
	}
	ofp = fopen(dst_file, "w");
	if (!ofp) {
		ret = errno; 
		fclose(ifp);
		return ret;
	}
	ret = copy_fd_to_fd(fileno(ifp), fileno(ofp));
	EXPECT_ZERO(ret);
	fclose(ofp);
	fclose(ifp);

	nbuf = calloc(1, buf_len + 1);
	EXPECT_NOT_EQUAL(nbuf, NULL);
	res = simple_io_read_whole_file(dst_file, nbuf, buf_len + 1);
	if (res < 0) {
		free(nbuf);
		return res;
	}
	if ((res > 0) && (memcmp(buf, nbuf, buf_len - 1))) {
		free(nbuf);
		return -EIO;
	}
	free(nbuf);
	return 0;
}

int main(void)
{
	char *bigbuf, tempdir[PATH_MAX];
	int next_id = 0;
	EXPECT_ZERO(get_tempdir(tempdir, sizeof(tempdir), 0700));
	EXPECT_ZERO(register_tempdir_for_cleanup(tempdir));

	EXPECT_ZERO(read_then_write_file(tempdir, &next_id, "", 0));
	EXPECT_ZERO(read_then_write_file(tempdir, &next_id, "abc", 123));
	EXPECT_ZERO(read_then_write_file(tempdir, &next_id,
					 "abracadabra", 5));
	EXPECT_ZERO(test_copy_fd_to_fd(tempdir, &next_id, ""));
	EXPECT_ZERO(test_copy_fd_to_fd(tempdir, &next_id, "foo bar"));
	bigbuf = malloc(BIGBUF_SZ);
	memset(bigbuf, 'm', BIGBUF_SZ - 1);
	bigbuf[BIGBUF_SZ - 1] = '\0';
	EXPECT_NOT_EQUAL(bigbuf, NULL);
	EXPECT_ZERO(test_copy_fd_to_fd(tempdir, &next_id, bigbuf));
	free(bigbuf);
	return EXIT_SUCCESS;
}
