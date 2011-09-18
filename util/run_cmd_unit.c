/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/compiler.h"
#include "util/run_cmd.h"
#include "util/safe_io.h"
#include "util/simple_io.h"
#include "util/test.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_run_cmd(void)
{
	int ret;
	ret = run_cmd("/bin/ls", "/", (char*)NULL);
	if (ret)
		return ret;
	ret = run_cmd("/bin/ls", (char*)NULL);
	if (ret)
		return ret;
	return 0;
}

static int test_run_cmd_get_output(void)
{
	int ret;
	char out[64];
	char expect[64];
	char bigdata[8096];
	const char *echo1[] = { "echo", "foo", "bar", "and", "baz", NULL };
	const char *echo2[] = { "echo", bigdata, NULL };
	memset(bigdata, 'Z', sizeof(bigdata));
	bigdata[sizeof(bigdata)-1] = '\0';

	ret = run_cmd_get_output(out, sizeof(out), echo1);
	if (ret) {
		fprintf(stderr, "run_cmd_get_output failed with error %d\n", ret);
		return ret;
	}
	if (strcmp(out, "foo bar and baz\n")) {
		fprintf(stderr, "test_run_cmd_get_output: echo1 failed. "
			"output was '%s'\n", out);
		return -EINVAL;
	}

	ret = run_cmd_get_output(out, sizeof(out), echo2);
	if (ret) {
		fprintf(stderr, "run_cmd_get_output failed with error %d\n", ret);
		return ret;
	}
	memset(expect, 'Z', sizeof(expect));
	expect[sizeof(expect)-1] = '\0';
	if (strcmp(out, expect)) {
		fprintf(stderr, "test_run_cmd_get_output: echo2 failed. "
			"output was '%s'; expected '%s'\n", out, expect);
		return -EINVAL;
	}
	return 0;
}

static int test_get_colocated_path(char *argv0)
{
	char buf1[4096], buf2[4096], path[PATH_MAX];
	ssize_t buf1_sz, buf2_sz;
	int ret;

	ret = get_colocated_path(argv0, "run_cmd_unit",
			   path, sizeof(path));
	buf1_sz = simple_io_read_whole_file(argv0, buf1, sizeof(buf1));
	if (buf1_sz < 0) {
		fprintf(stderr, "test_get_colocated_path: failed to read binary "
			"from '%s'\n", argv0);
		return buf1_sz;
	}
	buf2_sz = simple_io_read_whole_file(path, buf2, sizeof(buf2));
	if (buf2_sz < 0) {
		fprintf(stderr, "test_get_colocated_path: failed to read binary "
			"from '%s'\n", argv0);
		return buf1_sz;
	}
	if ((buf1_sz != buf2_sz) || (memcmp(buf1, buf2, buf1_sz))) {
		fprintf(stderr, "test_get_colocated_path: the binaries read "
			"from '%s' and '%s' were not the same.\n",
			argv0, path);
		return -EIO;
	}
	return 0;
}

static int test_run_cmd_give_input(const char *argv0, const char *str)
{
	int fd, pid, ret;
	char path[PATH_MAX];
	EXPECT_ZERO(get_colocated_path(argv0, "run_cmd_stdin_test",
			   path, sizeof(path)));
	{
		const char *cvec[] = { path, str, NULL };
		fd = start_cmd_give_input(cvec, &pid);
	}
	if (fd < 0) {
		fprintf(stderr, "start_cmd_give_input failed with "
			"error %d\n", fd);
		return fd;
	}
	EXPECT_ZERO(safe_write(fd, str, strlen(str) + 1));
	close(fd);
	ret = do_waitpid(pid);
	if (ret != 0) {
		fprintf(stderr, "do_waitpid returned %d\n", ret);
		return ret;
	}
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	EXPECT_ZERO(test_run_cmd());
	EXPECT_ZERO(test_run_cmd_get_output());
	EXPECT_ZERO(test_get_colocated_path(argv[0]));
	EXPECT_ZERO(test_run_cmd_give_input(argv[0], "abc"));
	EXPECT_ZERO(test_run_cmd_give_input(argv[0], ""));
	EXPECT_ZERO(test_run_cmd_give_input(argv[0], "foobarbaz"));

	return EXIT_SUCCESS;
}
