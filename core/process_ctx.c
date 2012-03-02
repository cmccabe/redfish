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
#include "core/glitch_log.h"
#include "core/pid_file.h"
#include "core/process_ctx.h"
#include "core/signal.h"
#include "jorm/jorm_const.h"
#include "util/bitfield.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_mgr.h"
#include "util/fast_log_types.h"

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

static void fast_log_to_core_log(POSSIBLY_UNUSED(void *log_ctx),
		const char *str)
{
	glitch_log("%s", str);
}

static void configure_fast_log(const char *redfish_log)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	BITFIELD_DECL(bits, FAST_LOG_TYPE_MAX);

	BITFIELD_ZERO(bits);
	if (redfish_log) {
		str_to_fast_log_bitfield(redfish_log, bits, err, err_len);
		if (err[0]) {
			glitch_log("configure_fast_log: %s\n", err);
		}
	}
	fast_log_mgr_set_storage_settings(g_fast_log_mgr, bits,
					  fast_log_to_core_log, NULL);
}

static int process_ctx_init_impl(const char *argv0, int daemonize,
				struct logc *lc)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	configure_glitch_log(lc);
	g_fast_log_mgr = fast_log_mgr_init(g_fast_log_dumpers);
	if (IS_ERR(g_fast_log_mgr)) {
		glitch_log("fast_log_mgr_init failed with error %d",
			PTR_ERR(g_fast_log_mgr));
		g_fast_log_mgr = NULL;
		goto error_close_glitchlog;
	}
	if (lc->fast_log == JORM_INVAL_STR)
		configure_fast_log(getenv("REDFISH_LOG"));
	else
		configure_fast_log(lc->fast_log);
	signal_init(argv0, err, err_len, lc);
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
	return 1;
}

int process_ctx_init(const char *argv0, int daemonize, struct logc *lc)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	harmonize_logc(lc, err, err_len, 1);
	if (err[0]) {
		glitch_log("log config error: %s\n", err);
		return 1;
	}
	return process_ctx_init_impl(argv0, daemonize, lc);
}

int utility_ctx_init(const char *argv0)
{
	struct logc lc;
	memset(&lc, 0, sizeof(lc));
	return process_ctx_init_impl(argv0, 0, &lc);
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
