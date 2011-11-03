/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/daemon_ctx.h"
#include "core/log_config.h"
#include "core/glitch_log.h"
#include "core/pid_file.h"
#include "core/signal.h"
#include "util/fast_log.h"
#include "util/fast_log_internal.h"
#include "util/fast_log_mgr.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int daemon_ctx_init(char *argv0, int daemonize, struct log_config *lc)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	harmonize_log_config(lc, err, err_len, 1, 1);
	if (err[0]) {
		glitch_log("log config error: %s\n", err);
		goto error;
	}
	configure_glitch_log(lc);
	signal_init(argv0, err, err_len, lc, NULL);
	if (err[0]) {
		glitch_log("signal_init error: %s\n", err);
		goto error_close_glitchlog;
	}
	if (daemonize) {
		if (daemon(0, 0) < 0) {
			int ret = errno;
			glitch_log("daemon(3) error %d\n", ret);
			goto error_signal_shutdown;
		}
	}
	create_pid_file(lc, err, err_len);
	if (err[0]) {
		glitch_log("create_pid_file error: %s\n", err);
		goto error_signal_shutdown;
	}
	return 0;

error_signal_shutdown:
	signal_shutdown();
error_close_glitchlog:
	close_glitch_log();
error:
	return 1;
}

void daemon_ctx_shutdown(void)
{
	signal_shutdown();
	close_glitch_log();
}
