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
#include "core/pid_file.h"
#include "core/process_ctx.h"
#include "core/signal.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/fast_log_mgr.h"
#include "util/fast_log_types.h"
#include "util/platform/signal.h"
#include "util/safe_io.h"

#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

typedef void (*sa_sigaction_t)(int, siginfo_t *, void *);

static sa_sigaction_t g_prev_handlers[_NSIG];

static const int FATAL_SIGNALS[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT,
	SIGTERM, SIGXCPU, SIGXFSZ, SIGSYS, SIGINT, SIGALRM };
static const int NUM_FATAL_SIGNALS =
	( sizeof(FATAL_SIGNALS) / sizeof(FATAL_SIGNALS[0]) );

static stack_t g_alt_stack;

static int g_crash_log_fd = -1;

static int g_fast_log_fd = -1;

static int g_use_syslog = 0;

static signal_cb_t g_fatal_signal_cb = NULL;

extern void regurgitate_fd(char *line, size_t max_line, int ifd, int ofd,
			int use_syslog);

/** Ths signal handler for nasty, fatal signals.
 */
static void handle_fatal_signal(int sig, siginfo_t *siginfo, void *ctx)
{
	int i, res, bsize;
	void *bentries[128];
	char *buf = (char*)bentries;

	for (i = 0; i < NUM_FATAL_SIGNALS; ++i) {
		signal(FATAL_SIGNALS[i], SIG_IGN);
	}
	if (g_fatal_signal_cb)
		g_fatal_signal_cb(sig);
	if (sig == SIGALRM) {
		snprintf(buf, sizeof(bentries), "%s ALARM EXPIRED\n",
			 (const char*)siginfo->si_value.sival_ptr);
		res = write(g_crash_log_fd, buf, strlen(buf));
	}
	snprintf(buf, sizeof(bentries), "HANDLE_FATAL_SIGNAL(sig=%d, "
			"name=%s)\n", sig, sys_siglist[sig]);
	res = write(g_crash_log_fd, buf, strlen(buf));
	signal_analyze_plat_data(ctx, buf, sizeof(bentries));
	res = write(g_crash_log_fd, buf, strlen(buf));

	bsize = backtrace(bentries, sizeof(bentries)/sizeof(bentries[0]));
	backtrace_symbols_fd(bentries, bsize, g_crash_log_fd);
	snprintf(buf, sizeof(bentries), "END_HANDLE_FATAL_SIGNAL\n");
	res = write(g_crash_log_fd, buf, strlen(buf));
	fsync(g_crash_log_fd);

	if (g_crash_log_fd != STDERR_FILENO) {
		/* This function reads the file we just wrote, and sends it to
		 * some other output streams.
		 *
		 * We always write it to stderr. Most of the time, stderr will
		 * be hooked to /dev/null, but sometimes it isn't.
		 *
		 * If use_syslog was specified, we write the file to syslog.
		 * Technically, we're not supposed to do this, because syslog is
		 * not a signal-safe function.  However, we've already written
		 * out the crash_log, so there will be some record of what went
		 * wrong, even if we crash and burn in this function.  Also, in
		 * practice, syslog tends to do non-signal-safe stuff like call
		 * malloc only on the first invocation or when openlog is
		 * called.
		 */
		regurgitate_fd((char*)bentries, sizeof(bentries),
				 g_crash_log_fd, -1, g_use_syslog);
	}
	/* If the pid file has not been set up, this will do nothing. */
	delete_pid_file();
	/* dump fast logs */
	fast_log_mgr_dump_all(g_fast_log_mgr, g_fast_log_fd);
	snprintf(buf, sizeof(bentries), "END_FAST_LOG_DUMP\n");
	res = write(g_crash_log_fd, buf, strlen(buf));
	fsync(g_crash_log_fd);
	/* Call the previously install signal handler.
	 * Probably, this will dump core. */
	g_prev_handlers[sig](sig, siginfo, ctx);
}

static void signal_init_altstack(char *err, size_t err_len,
				stack_t *alt_stack)
{
	/* Set up alternate stack */
	alt_stack->ss_sp = calloc(1, SIGSTKSZ);
	if (!alt_stack->ss_sp) {
		snprintf(err, err_len, "signal_init_altstack: failed to "
			"allocate %d bytes for alternate stack.", SIGSTKSZ);
		return;
	}
	alt_stack->ss_size = SIGSTKSZ;
	if (sigaltstack(alt_stack, NULL)) {
		int ret = errno;
		snprintf(err, err_len, "signal_init_altstack: sigaltstack "
			 "failed with error %d", ret);
		free(alt_stack->ss_sp);
		return;
	}
}

static void signal_set_dispositions(char *err, size_t err_len)
{
	int ret, i;
	struct sigaction sigact;
	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_sigaction = handle_fatal_signal;
	sigact.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;

	for (i = 0; i < NUM_FATAL_SIGNALS; i++) {
		if (FATAL_SIGNALS[i] >= _NSIG) {
			snprintf(err, err_len, "FATAL_SIGNALS[%d] = %d, "
				"which exceeds _NSIG=%d !\n",
				i, FATAL_SIGNALS[i], _NSIG);
			return;
		}
	}
	for (i = 0; i < NUM_FATAL_SIGNALS; i++) {
		struct sigaction oldact;
		memset(&oldact, 0, sizeof(oldact));
		ret = sigaction(FATAL_SIGNALS[i], &sigact, &oldact);
		if (ret == -1) {
			ret = errno;
			snprintf(err, err_len, "signal_set_dispositions: "
				"sigaction(%d) failed with error %d", i, ret);
			return;
		}
		g_prev_handlers[FATAL_SIGNALS[i]] = oldact.sa_sigaction;
	}

	/* Always ignore SIGPIPE; it is annoying. */
	sigact.sa_sigaction = NULL;
	ret = sigaction(SIGPIPE, &sigact, NULL);
	if (ret == -1) {
		ret = errno;
		snprintf(err, err_len, "signal_set_dispositions: "
			"signal(SIGPIPE) failed with error %d", ret);
		return;
	}
}

static int should_close_fd(int fd)
{
	if (fd < 0)
		return 0;
	if ((fd == STDOUT_FILENO) ||
		    (fd == STDERR_FILENO) ||
		    (fd == STDIN_FILENO))
		return 0;
	return 1;
}

void signal_shutdown(void)
{
	int res, i;
	for (i = 0; i < NUM_FATAL_SIGNALS; ++i) {
		signal(FATAL_SIGNALS[i], SIG_DFL);
	}
	signal(SIGPIPE, SIG_DFL);
	free(g_alt_stack.ss_sp);
	g_alt_stack.ss_sp = NULL;
	if (should_close_fd(g_crash_log_fd))
		RETRY_ON_EINTR(res, close(g_crash_log_fd));
	g_crash_log_fd = -1;
	if (should_close_fd(g_fast_log_fd))
		RETRY_ON_EINTR(res, close(g_fast_log_fd));
	g_fast_log_fd = -1;
	g_use_syslog = 0;
	g_fatal_signal_cb = NULL;
}

void signal_init(const char *argv0, char *err, size_t err_len,
		 const struct logc *lc, signal_cb_t fatal_signal_cb)
{
	if ((g_alt_stack.ss_sp != NULL) || (g_crash_log_fd != -1)) {
		snprintf(err, err_len, "signal_init: already "
			 "initialized!");
		return;
	}
	if (lc->crash_log) {
		g_crash_log_fd = open(lc->crash_log,
			O_CREAT | O_TRUNC | O_RDWR, 0640);
		if (g_crash_log_fd < 0) {
			int ret = errno;
			snprintf(err, err_len, "signal_init: open(%s) "
				 "failed: error %d", lc->crash_log, ret);
			signal_shutdown();
			return;
		}
	}
	else {
		g_crash_log_fd = STDERR_FILENO;
	}
	if (lc->fast_log) {
		g_fast_log_fd = open(lc->fast_log,
			O_CREAT | O_TRUNC | O_WRONLY, 0640);
		if (g_fast_log_fd < 0) {
			int ret = errno;
			snprintf(err, err_len, "signal_init: open(%s) "
				 "failed: error %d", lc->crash_log, ret);
			signal_shutdown();
			return;
		}
	}
	else {
		g_fast_log_fd = STDERR_FILENO;
	}
	g_fatal_signal_cb = fatal_signal_cb;
	signal_init_altstack(err, err_len, &g_alt_stack);
	if (err[0]) {
		signal_shutdown();
		return;
	}
	signal_set_dispositions(err, err_len);
	if (err[0]) {
		signal_shutdown();
		return;
	}
	if (lc->use_syslog) {
		g_use_syslog = 1;
		openlog(argv0, LOG_NDELAY | LOG_PID, LOG_USER);
	}
	else {
		g_use_syslog = 0;
	}
}
