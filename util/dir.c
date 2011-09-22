/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/dir.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void do_mkdir(const char *dir_name, int mode, char *err, size_t err_len)
{
	struct stat st_buf;
	int ret;
	memset(&st_buf, 0, sizeof(struct stat));
	if (stat(dir_name, &st_buf) == 0) {
		if (S_ISDIR(st_buf.st_mode) == 0) {
			snprintf(err, err_len, "base directory '%s' already exists, "
				 "but not as a directory!", dir_name);
			return;
		}
		return;
	}
	ret = errno;
	if (ret != ENOENT) {
		snprintf(err, err_len, "error trying to stat '%s': error %d\n",
			 dir_name, ret);
		return;
	}
	if (mkdir(dir_name, mode)) {
		snprintf(err, err_len, "error trying to create '%s': error %d\n",
			 dir_name, ret);
		return;
	}
}

int do_mkdir_p(const char *path, int mode)
{
	int ret;
	char *str, full[PATH_MAX], tbuf[PATH_MAX];

	full[0] = '\0';
	strcpy(tbuf, path);
	str = tbuf;
	while (1) {
		char *tmp, *seg;
		seg = strtok_r(str, "/", &tmp);
		if (!seg)
			break;
		str = NULL;
		strcat(full, "/");
		strcat(full, seg);
		if (mkdir(full, mode) < 0) {
			struct stat sbuf;
			ret = errno;
			if (ret != EEXIST)
				return ret;
			if (stat(full, &sbuf))
				return -errno;
			if (!S_ISDIR(sbuf.st_mode))
				return -ENOTDIR;
		}
	}
	return 0;
}
