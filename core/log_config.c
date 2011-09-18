/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/log_config.h"
#include "util/string.h"

#define JORM_CUR_FILE "core/log_config.jorm"
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
			snprintf(err, err_len, "log_config error: path for %s "
				 "is longer than PATH_MAX!", conf_name);
			return;
		}
		return;
	}
	if (base_dir == JORM_INVAL_STR) {
		snprintf(err, err_len, "log_config error: you must specify "
			 "%s, or set base_dir.\n", conf_name);
		return;
	}
	if (zsnprintf(path, PATH_MAX, "%s/%s", base_dir, default_name)) {
		snprintf(err, err_len, "log_config error: base_dir + "
			 "default_name is longer than PATH_MAX!");
		return;
	}
	*cstr = strdup(path);
	if (!*cstr) {
		snprintf(err, err_len, "log_config error: out of memory.");
		return;
	}
}

struct log_config *create_log_config(struct json_object *jo,
		char *err, size_t err_len, enum fish_daemon_ty dty)
{
	struct log_config *lc = JORM_FROMJSON_log_config(jo);
	if (!lc) {
		snprintf(err, err_len, "JORM_FROMJSON_log_config: out "
			 "of memory.\n");
		goto error;
	}
	if (lc->use_syslog == JORM_INVAL_BOOL)
		lc->use_syslog = 0;
	resolve_log_name("crash_log", "crash.log", &lc->crash_log,
			 lc->base_dir, err, err_len);
	if (err[0])
		goto error_free_lconf;
	resolve_log_name("glitch_log", "glitch.log", &lc->glitch_log,
			 lc->base_dir, err, err_len);
	if (err[0])
		goto error_free_lconf;
	if (dty == ONEFISH_DAEMON_TYPE_MON) {
		resolve_log_name("mon_data_dir", "data", &lc->mon_data_dir,
				 lc->base_dir, err, err_len);
		if (err[0])
			goto error_free_lconf;
	}
	resolve_log_name("pid_file", "pid", &lc->pid_file,
			 lc->base_dir, err, err_len);
	if (err[0])
		goto error_free_lconf;
	if (dty == ONEFISH_DAEMON_TYPE_MON) {
		resolve_log_name("socket_path", "socket", &lc->socket_path,
				 lc->base_dir, err, err_len);
		if (err[0])
			goto error_free_lconf;
	}
	return lc;

error_free_lconf:
	JORM_FREE_log_config(lc);
error:
	return NULL;
}

void free_log_config(struct log_config *lconf)
{
	JORM_FREE_log_config(lconf);
}
