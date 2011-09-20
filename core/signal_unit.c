/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/log_config.h"
#include "core/signal.h"
#include "util/error.h"
#include "util/simple_io.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

static int validate_crash_log(const char *crash_log, int sig)
{
	int ret;
	char prefix[256], expected_prefix[256];
	snprintf(expected_prefix, sizeof(expected_prefix),
		"HANDLE_FATAL_SIGNAL(sig=%d, name=", sig);
	memset(prefix, 0, sizeof(prefix));
	ret = simple_io_read_whole_file(crash_log, prefix, sizeof(prefix));
	if (ret < 0) {
		fprintf(stderr, "failed to open crash log file '%s': "
			"error %d\n", crash_log, ret);
		return ret;
	}
	if (strncmp(expected_prefix, prefix, strlen(expected_prefix))) {
		fprintf(stderr, "expected: %s\ngot: %s\n",
			expected_prefix, prefix);
		return -EDOM;
	}
	return 0;
}

static int test_signal_handler(const char *tempdir, int sig)
{
	int ret, pid, status;
	char err[512] = { 0 };
	char crash_log[PATH_MAX];
	snprintf(crash_log, sizeof(crash_log), "%s/crash.log.%d",
		 tempdir, rand());
	pid = fork();
	if (pid == -1) {
		ret = errno;
		return ret;
	}
	else if (pid == 0) {
		struct log_config lc;
		memset(&lc, 0, sizeof(lc));
		lc.crash_log = crash_log;
		signal_init(err, sizeof(err), &lc, NULL);
		if (err[0]) {
			fprintf(stderr, "signal_init error: %s\n", err);
			_exit(1);
		}
		raise(sig);
		_exit(1);
	}
	RETRY_ON_EINTR(ret, waitpid(pid, &status, 0));

	EXPECT_ZERO(validate_crash_log(crash_log, sig));
	return 0;
}

int main(void)
{
	char tempdir[PATH_MAX];
	srand(time(NULL));
	EXPECT_ZERO(get_tempdir(tempdir, sizeof(tempdir), 0770));
	EXPECT_ZERO(register_tempdir_for_cleanup(tempdir));
	EXPECT_ZERO(test_signal_handler(tempdir, SIGSEGV));
	EXPECT_ZERO(test_signal_handler(tempdir, SIGBUS));
	EXPECT_ZERO(test_signal_handler(tempdir, SIGILL));
	EXPECT_ZERO(test_signal_handler(tempdir, SIGFPE));
	EXPECT_ZERO(test_signal_handler(tempdir, SIGABRT));
	EXPECT_ZERO(test_signal_handler(tempdir, SIGINT));
	return EXIT_SUCCESS;
}
