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

#include "common/config/logc.h"
#include "core/process_ctx.h"
#include "core/signal.h"
#include "util/error.h"
#include "util/fast_log_mgr.h"
#include "util/fast_log_types.h"
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
	ret = simple_io_read_whole_file_zt(crash_log, prefix, sizeof(prefix));
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

static int test_signal_handler(const char *argv0, const char *tempdir, int sig)
{
	int ret, pid, status;
	char err[512] = { 0 };
	char crash_log_path[PATH_MAX];
	snprintf(crash_log_path, sizeof(crash_log_path), "%s/crash.log.%d",
		 tempdir, rand());
	pid = fork();
	if (pid == -1) {
		ret = errno;
		return ret;
	}
	else if (pid == 0) {
		struct logc lc;
		memset(&lc, 0, sizeof(lc));
		lc.crash_log_path = crash_log_path;
		signal_init(argv0, err, sizeof(err), &lc, NULL);
		if (err[0]) {
			fprintf(stderr, "signal_init error: %s\n", err);
			_exit(1);
		}
		raise(sig);
		_exit(1);
	}
	RETRY_ON_EINTR(ret, waitpid(pid, &status, 0));

	EXPECT_ZERO(validate_crash_log(crash_log_path, sig));
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	char tempdir[PATH_MAX];
	srand(time(NULL));

	g_fast_log_mgr = fast_log_mgr_init(g_fast_log_dumpers);
	EXPECT_NOT_EQ(g_fast_log_mgr, NULL);
	EXPECT_ZERO(get_tempdir(tempdir, sizeof(tempdir), 0770));
	EXPECT_ZERO(register_tempdir_for_cleanup(tempdir));
	EXPECT_ZERO(test_signal_handler(argv[0], tempdir, SIGSEGV));
	EXPECT_ZERO(test_signal_handler(argv[0], tempdir, SIGBUS));
	EXPECT_ZERO(test_signal_handler(argv[0], tempdir, SIGILL));
	EXPECT_ZERO(test_signal_handler(argv[0], tempdir, SIGFPE));
	EXPECT_ZERO(test_signal_handler(argv[0], tempdir, SIGABRT));
	EXPECT_ZERO(test_signal_handler(argv[0], tempdir, SIGINT));
	fast_log_mgr_free(g_fast_log_mgr);
	return EXIT_SUCCESS;
}
