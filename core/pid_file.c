/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/log_config.h"
#include "core/pid_file.h"
#include "util/error.h"
#include "util/safe_io.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static char g_pid_file[PATH_MAX];

void create_pid_file(const struct log_config *lc, char *err, size_t err_len)
{
	char pid_buf[512];
	int fd, ret, res;
	uint64_t pid;

	if (lc->pid_file == NULL)
		return; 
	if (g_pid_file[0]) {
		snprintf(err, err_len, "create_pid_file was called twice!");
		return;
	}
	pid = getpid();
	snprintf(pid_buf, sizeof(pid_buf), "%"PRId64"\n", pid);
	fd = open(lc->pid_file, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd < 0) {
		ret = errno;
		snprintf(err, err_len, "failed to open pid file '%s': "
			 "error %d", lc->pid_file, ret);
		return;
	}
	ret = safe_write(fd, pid_buf, strlen(pid_buf));
	RETRY_ON_EINTR(res, close(fd));
	if (ret) {
		snprintf(err, err_len, "failed to write pid file '%s': "
			 "error %d", lc->pid_file, ret);
		return;
	}
	snprintf(g_pid_file, PATH_MAX, "%s", lc->pid_file);
	atexit(delete_pid_file);
}

void delete_pid_file(void)
{
	if (g_pid_file[0] == '\0')
		return;
	unlink(g_pid_file);
}
