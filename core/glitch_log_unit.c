/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/glitch_log.h"
#include "core/log_config.h"
#include "util/simple_io.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TEST_STR "I can see my house from here!\n"
#define TEST_STR2 "Second string!\n"

static int test_stderr_output(const char *tempdir)
{
	char buf[4096];
	int fd, ret;
	char tempfile[PATH_MAX];

	/* Redirect stderr to a file in our temporary directory. */
	snprintf(tempfile, sizeof(tempfile), "%s/stderr", tempdir);
	fd = open(tempfile, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	if (fd == -1) {
		int err = errno;
		fprintf(stderr, "open(%s) err = %d\n", tempfile, err);
		return -err;
	}
	ret = dup2(fd, STDERR_FILENO);
	if (ret == -1) {
		int err = errno;
		printf("dup2 err = %d\n", err);
		return -err;
	}
	glitch_log(TEST_STR);
	ret = simple_io_read_whole_file_zt(tempfile, buf, sizeof(buf));
	if (ret < 0) {
		printf("simple_io_read_whole_file_zt(%s) failed: error %d\n",
		       tempfile, ret);
		return ret;
	}
	if (strcmp(buf, TEST_STR) != 0) {
		printf("read '%s'; expected to find '" TEST_STR "' in "
			"it.\n", buf);
		return -EDOM;
	}
	return 0;
}

static int test_log_output(const char *log)
{
	char expected_buf[4096];
	char buf[4096];

	glitch_log(TEST_STR2);

	/* Check the log file that should be written */
	EXPECT_POSITIVE(simple_io_read_whole_file_zt(log, buf, sizeof(buf)));
	snprintf(expected_buf, sizeof(expected_buf), "%s%s",
		 TEST_STR, TEST_STR2);
	if (strcmp(buf, expected_buf) != 0) {
		printf("read '%s'; expected '%s'\n", buf, expected_buf);
		return -EDOM;
	}
	return 0;
}

int main(void)
{
	char tempdir[PATH_MAX];
	char glitch_log[PATH_MAX];
	struct log_config lc;

	EXPECT_ZERO(get_tempdir(tempdir, PATH_MAX, 0700));
	EXPECT_ZERO(register_tempdir_for_cleanup(tempdir));
	snprintf(glitch_log, sizeof(glitch_log), "%s/glitch_log.txt", tempdir);
	memset(&lc, 0, sizeof(struct log_config));
	lc.glitch_log = glitch_log;
	EXPECT_ZERO(test_stderr_output(tempdir));
	configure_glitch_log(&lc);
	EXPECT_ZERO(test_log_output(lc.glitch_log));
	return EXIT_SUCCESS;
}
