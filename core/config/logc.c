/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/config/logc.h"
#include "util/dir.h"
#include "util/string.h"

#define JORM_CUR_FILE "core/config/logc.jorm"
#include "jorm/jorm_generate_body.h"
#undef JORM_CUR_FILE

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void resolve_log_name(const char *conf_name, const char *default_name,
	char **cstr, const char *base_dir, char *err, size_t err_len)
{
	char path[PATH_MAX];
	if (*cstr != JORM_INVAL_STR) {
		if (strlen(*cstr) > PATH_MAX) {
			snprintf(err, err_len, "logc error: path for %s "
				 "is longer than PATH_MAX!", conf_name);
			return;
		}
		return;
	}
	if (base_dir == JORM_INVAL_STR) {
		snprintf(err, err_len, "logc error: you must specify "
			 "%s, or set base_dir.\n", conf_name);
		return;
	}
	if (zsnprintf(path, PATH_MAX, "%s/%s", base_dir, default_name)) {
		snprintf(err, err_len, "logc error: base_dir + "
			 "default_name is longer than PATH_MAX!");
		return;
	}
	*cstr = strdup(path);
	if (!*cstr) {
		snprintf(err, err_len, "logc error: out of memory.");
		return;
	}
}

void harmonize_logc(struct logc *lc,
		char *err, size_t err_len, int want_mkdir)
{
	if (lc->use_syslog == JORM_INVAL_BOOL)
		lc->use_syslog = 0;
	resolve_log_name("crash_log", "crash.log", &lc->crash_log,
			 lc->base_dir, err, err_len);
	if (err[0])
		return;
	resolve_log_name("fast_log", "fast.log", &lc->fast_log,
			 lc->base_dir, err, err_len);
	if (err[0])
		return;
	resolve_log_name("glitch_log", "glitch.log", &lc->glitch_log,
			 lc->base_dir, err, err_len);
	if (err[0])
		return;
	resolve_log_name("pid_file", "pid", &lc->pid_file,
			 lc->base_dir, err, err_len);
	if (err[0])
		return;
	if (want_mkdir) {
		if (lc->base_dir) {
			int ret = do_mkdir_p(lc->base_dir, 0755);
			if (ret) {
				snprintf(err, err_len, "do_mkdir(%s) "
					 "returned %d\n", lc->base_dir, ret);
				return;
			}
		}
	}
}
