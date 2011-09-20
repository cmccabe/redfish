/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/log_config.h"
#include "core/signal.h"
#include "util/compiler.h"
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

static const int FATAL_SIGNALS[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT,
	SIGXCPU, SIGXFSZ, SIGSYS, SIGINT };
static const int NUM_FATAL_SIGNALS = (sizeof(FATAL_SIGNALS) / sizeof(FATAL_SIGNALS[0]));

static stack_t g_alt_stack;

static int g_crash_log_fd = -1;

static int g_use_syslog = 0;

static signal_cb_t g_fatal_signal_cb = NULL;

/** This function reads the file we just wrote, and sends it to some other
 * output streams.
 *
 * We always write it to stderr. Most of the time, stderr will be hooked to
 * /dev/null, but sometimes it isn't.
 *
 * If use_syslog was specified, we write the file to syslog. Technically, we're
 * not supposed to do this, because syslog is not a signal-safe function.
 * However, we've already written out the crash_log, so there will be some
 * record of what went wrong, even if we crash and burn in this function.
 * Also, in practice, syslog tends to do non-signal-safe stuff like call malloc
 * only on the first invocation or when openlog is called.
 */
static void regurgitate_fd(char *line, size_t max_line, int fd)
{
	int ret;
	size_t bidx;
	ret = lseek(fd, 0, SEEK_SET);
	if (ret)
		return;
	memset(line, 0, max_line);
	bidx = 0;
	while (1) {
		char b[1];
		int tmp, res = read(fd, b, 1);
		if ((bidx == max_line - 2) || (res <= 0) || (b[0] == '\n')) {
			if (g_use_syslog) {
				syslog(LOG_ERR | LOG_USER, "%s", line);
			}
			line[bidx++] = '\n';
			tmp = safe_write(STDERR_FILENO, line, bidx);
			if (res <= 0)
				break;
			memset(line, 0, max_line);
			bidx = 0;
		}
		else {
			line[bidx++] = b[0];
		}
	}
}

/** Ths signal handler for nasty, fatal signals.
 */
static void handle_fatal_signal(int sig,
	POSSIBLY_UNUSED(siginfo_t *siginfo), POSSIBLY_UNUSED(void *ctx))
{
	int i, res, bsize;
	void *bentries[128];
	char *buf = (char*)bentries;

	for (i = 0; i < NUM_FATAL_SIGNALS; ++i) {
		signal(FATAL_SIGNALS[i], SIG_IGN);
	}
	if (g_fatal_signal_cb)
		g_fatal_signal_cb(sig);
	snprintf(buf, sizeof(bentries), "HANDLE_FATAL_SIGNAL(sig=%d, name=%s)\n",
		sig, sys_siglist[sig]);
	res = write(g_crash_log_fd, buf, strlen(buf));
	bsize = backtrace(bentries, sizeof(bentries)/sizeof(bentries[0]));
	backtrace_symbols_fd(bentries, bsize, g_crash_log_fd);
	snprintf(buf, sizeof(bentries), "END_HANDLE_FATAL_SIGNAL\n");
	res = write(g_crash_log_fd, buf, strlen(buf));
	fsync(g_crash_log_fd);

	if (g_crash_log_fd != STDERR_FILENO) {
		regurgitate_fd((char*)bentries, sizeof(bentries),
				 g_crash_log_fd);
	}
	/* die */
	raise(sig);
}

static void signal_init_altstack(char *error, size_t error_len,
				stack_t *alt_stack)
{
	/* Set up alternate stack */
	alt_stack->ss_sp = calloc(1, SIGSTKSZ);
	if (!alt_stack->ss_sp) {
		snprintf(error, error_len, "signal_init_altstack: failed to "
			"allocate %d bytes for alternate stack.", SIGSTKSZ);
		return;
	}
	alt_stack->ss_size = SIGSTKSZ;
	if (sigaltstack(alt_stack, NULL)) {
		int err = errno;
		snprintf(error, error_len, "signal_init_altstack: sigaltstack "
			 "failed with error %d", err);
		free(alt_stack->ss_sp);
		return;
	}
}

static void signal_set_dispositions(char *error, size_t error_len)
{
	int ret, i;
	struct sigaction sigact;
	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_sigaction = handle_fatal_signal;
	/* TODO: support/use SA_SIGINFO? */
	sigact.sa_flags = SA_RESETHAND | SA_ONSTACK;

	for (i = 0; i < NUM_FATAL_SIGNALS; i++) {
		ret = sigaction(FATAL_SIGNALS[i], &sigact, NULL);
		if (ret == -1) {
			ret = errno;
			snprintf(error, error_len, "signal_set_dispositions: "
				"sigaction(%d) failed with error %d", i, ret);
			signal_resset_dispositions();
			return;
		}
	}

	/* Always ignore SIGPIPE; it is annoying. */
	sigact.sa_sigaction = NULL;
	ret = sigaction(SIGPIPE, &sigact, NULL);
	if (ret == -1) {
		ret = errno;
		snprintf(error, error_len, "signal_set_dispositions: "
			"signal(SIGPIPE) failed with error %d", ret);
		signal_resset_dispositions();
		return;
	}
}

void signal_resset_dispositions(void)
{
	int i;
	for (i = 0; i < NUM_FATAL_SIGNALS; ++i) {
		signal(FATAL_SIGNALS[i], SIG_IGN);
	}
	free(g_alt_stack.ss_sp);
	if (g_crash_log_fd != -1) {
		close(g_crash_log_fd);
		g_crash_log_fd = -1;
	}
}

void signal_init(const char *argv0, char *err, size_t err_len,
		 const struct log_config *lc, signal_cb_t fatal_signal_cb)
{
	int ret;

	if ((g_alt_stack.ss_sp != NULL) || (g_crash_log_fd != -1)) {
		snprintf(err, err_len, "signal_init: already "
			 "initialized!");
		return;
	}
	if (lc->crash_log) {
		g_crash_log_fd = open(lc->crash_log,
			O_CREAT | O_TRUNC | O_RDWR, 0640);
		if (g_crash_log_fd < 0) {
			ret = errno;
			snprintf(err, err_len, "signal_init: open(%s) "
				 "failed: error %d", lc->crash_log, ret);
			return;
		}
	}
	else {
		g_crash_log_fd = STDERR_FILENO;
	}
	g_fatal_signal_cb = fatal_signal_cb;
	signal_init_altstack(err, err_len, &g_alt_stack);
	if (err[0])
		return;
	signal_set_dispositions(err, err_len);
	if (err[0])
		return;
	if (lc->use_syslog) {
		g_use_syslog = 1;
		openlog(argv0, LOG_NDELAY | LOG_PID, LOG_USER);
	}
	else {
		g_use_syslog = 0;
	}
}
