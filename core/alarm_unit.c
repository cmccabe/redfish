/*
 * vim: ts=8:sw=8:tw=79:noet
 * 
 * Copyright 2012 the Redfish authors
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

#include "core/alarm.h"
#include "core/process_ctx.h"
#include "util/error.h"
#include "util/macro.h"
#include "util/safe_io.h"
#include "util/platform/pipe2.h"
#include "util/test.h"
#include "util/time.h"

#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int alarm_unit_setup_disarm(void)
{
	time_t cur;
	timer_t timer;

	cur = mt_time();
	EXPECT_ZERO(mt_set_alarm(cur + 10000, TO_STR2(__LINE__), &timer));
	EXPECT_ZERO(mt_deactivate_alarm(timer));
	return 0;
}

static int g_alarm_unit_pipefd[2] = { -1, -1 };

void alarm_unit_test2_sig_handler(int sig_num)
{
	char buf[1] = { 0 };

	if (sig_num != SIGALRM)
		abort();
	/* If we thought that this signal handler would ever execute more than
	 * once, we'd have to use a non-blocking pipe here.  Don't copy this
	 * code unless you understand what I'm talking about here.
	 */
	if (write(g_alarm_unit_pipefd[PIPE_WRITE], buf, 1) != 1)
		abort();
}

static int alarm_unit_test2(void)
{
	time_t cur;
	char buf[1];
	timer_t timer, timer2;

	cur = mt_time();
	EXPECT_ZERO(mt_set_alarm(cur + 1, TO_STR2(__LINE__), &timer));
	EXPECT_ZERO(mt_set_alarm(cur + 10000, TO_STR2(__LINE__), &timer2));
	EXPECT_ZERO(mt_deactivate_alarm(timer2));
	EXPECT_ZERO(safe_read_exact(g_alarm_unit_pipefd[PIPE_READ], buf, 1));
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	int res;

	EXPECT_ZERO(utility_ctx_init(argv[0])); 
	EXPECT_ZERO(do_pipe2(g_alarm_unit_pipefd, WANT_O_CLOEXEC));
	signal(SIGALRM, alarm_unit_test2_sig_handler);
	EXPECT_ZERO(alarm_unit_setup_disarm());
	EXPECT_ZERO(alarm_unit_test2());
	RETRY_ON_EINTR(res, close(g_alarm_unit_pipefd[PIPE_READ]));
	RETRY_ON_EINTR(res, close(g_alarm_unit_pipefd[PIPE_WRITE]));
	process_ctx_shutdown();

	return EXIT_SUCCESS;
}
