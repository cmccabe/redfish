/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/process_ctx.h"
#include "core/log_config.h"
#include "core/glitch_log.h"
#include "core/pid_file.h"
#include "core/signal.h"
#include "util/bitfield.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_types.h"
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

struct fast_log_mgr *g_fast_log_mgr;

static void fast_log_to_core_log(char *buf)
{
	glitch_log("%s", buf);
}

static void configure_fast_log(void)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	const char *f;
	BITFIELD_DECL(bits, FAST_LOG_TYPE_MAX);

	BITFIELD_ZERO(bits);
	f = getenv("FAST_LOG");
	if (f) {
		str_to_fast_log_bitfield(f, bits, err, err_len);
		if (err[0]) {
			glitch_log("configure_fast_log: %s\n", err);
		}
	}
	fast_log_mgr_set_storage_settings(g_fast_log_mgr, bits,
					  fast_log_to_core_log);
}

int process_ctx_init(char *argv0, int daemonize, struct log_config *lc)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	harmonize_log_config(lc, err, err_len, 1, 1);
	if (err[0]) {
		glitch_log("log config error: %s\n", err);
		goto error;
	}
	configure_glitch_log(lc);
	g_fast_log_mgr = fast_log_mgr_init(g_fast_log_dumpers);
	if (IS_ERR(g_fast_log_mgr)) {
		glitch_log("fast_log_mgr_init failed with error %d",
			PTR_ERR(g_fast_log_mgr));
		g_fast_log_mgr = NULL;
		goto error_close_glitchlog;
	}
	configure_fast_log();
	signal_init(argv0, err, err_len, lc, NULL);
	if (err[0]) {
		glitch_log("signal_init error: %s\n", err);
		goto error_free_fast_log_mgr;
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
error_free_fast_log_mgr:
	fast_log_mgr_free(g_fast_log_mgr);
	g_fast_log_mgr = NULL;
error_close_glitchlog:
	close_glitch_log();
error:
	return 1;
}

void process_ctx_shutdown(void)
{
	signal_shutdown();
	if (g_fast_log_mgr) {
		fast_log_mgr_free(g_fast_log_mgr);
		g_fast_log_mgr = NULL;
	}
	close_glitch_log();
}
