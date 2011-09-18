/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/signal.h"
#include "util/compiler.h"

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

static signal_cb_t g_fatal_signal_cb = NULL;

static void cat_fd_to_syslog(char *line, size_t max_line, int fd)
{
	int ret;
	size_t bidx;
	/* Now write the file we just wrote to syslog.  We can't use fopen and
	 * friends, because they might allocate memory.  So we just read lines
	 * one byte at a time and then send them off to syslog.  It would be
	 * nice if we could use backtrace_symbols here, but we can't. That
	 * function calls malloc, for reasons that can best be described as
	 * "misguided." */
	ret = lseek(fd, 0, SEEK_SET);
	if (ret)
		return;
	memset(line, 0, max_line);
	bidx = 0;
	while (1) {
		char b[1];
		int res = read(fd, b, 1);
		if ((bidx == max_line - 1) || (res <= 0) || (b[0] == '\n')) {
			syslog(LOG_ERR | LOG_USER | LOG_PERROR, "%s", line);
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
	int res, bsize;
	void *bentries[128];
	char *buf = (char*)bentries;
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
		cat_fd_to_syslog((char*)bentries, sizeof(bentries),
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

void signal_init(char *error, size_t error_len, const char *crash_log,
		 signal_cb_t fatal_signal_cb)
{
	int ret;

	if ((g_alt_stack.ss_sp != NULL) || (g_crash_log_fd != -1)) {
		snprintf(error, error_len, "signal_init: already "
			 "initialized!");
		return;
	}
	if (crash_log) {
		g_crash_log_fd = open(crash_log,
			O_CREAT | O_TRUNC | O_RDWR, 0600);
		if (g_crash_log_fd < 0) {
			ret = errno;
			snprintf(error, error_len, "signal_init: open(%s) "
				 "failed: error %d", crash_log, ret);
			return;
		}
	}
	else {
		g_crash_log_fd = STDERR_FILENO;
	}
	g_fatal_signal_cb = fatal_signal_cb;
	signal_init_altstack(error, error_len, &g_alt_stack);
	if (error[0])
		return;
	signal_set_dispositions(error, error_len);
	if (error[0])
		return;
}
