/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

/*
 * There is no locking in this file.
 * All operations should be re-entrant!
 */

#include "chunk/io.h"
#include "util/error.h"
#include "util/safe_io.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ONECHUNK_PATH_SUFFIX_SZ (sizeof("01/23456789abcdef"))

static char onechunk_prefix[PATH_MAX] = { 0 };

int onechunk_set_prefix(const char *prefix)
{
	if (strlen(prefix) + ONECHUNK_PATH_SUFFIX_SZ + 2 > PATH_MAX) {
		return -ENAMETOOLONG;
	}
	snprintf(onechunk_prefix, PATH_MAX, "%s", prefix);
	return 0;
}

int onechunk_write(uint64_t bid, const char *data, int count, int offset)
{
	int ret, fd, tmp;
	char path[PATH_MAX];
	while (1) {
		snprintf(path, PATH_MAX, "%s/%02x/%014" PRIx64, onechunk_prefix,
			 (int)(bid & 0xff), (bid >> 16));
		fd = open(path, O_APPEND | O_WRONLY | O_CREAT, 0660);
		if (fd != -1)
			break;
		ret = errno;
		if (ret != ENOENT)
			return ret;
		snprintf(path, PATH_MAX, "%s/%02x", onechunk_prefix,
			 (int)(bid & 0xff));
		if (mkdir(path, 0770) == -1) {
			ret = errno;
			if (ret == EEXIST)
				continue;
			return ret;
		}
	}
	ret = safe_pwrite(fd, data, count, offset);
	RETRY_ON_EINTR(tmp, close(fd));
	return ret;
}

int onechunk_read(uint64_t bid, char *data, int count, int offset)
{
	int ret, fd, tmp;
	char path[PATH_MAX];
	snprintf(path, PATH_MAX, "%s/%02x/%014" PRIx64, onechunk_prefix,
		 (int)(bid & 0xff), (bid >> 16));
	fd = open(path, O_APPEND | O_RDONLY, 0660);
	if (fd == -1) {
		ret = errno;
		return -ret;
	}
	ret = safe_pread(fd, data, count, offset);
	RETRY_ON_EINTR(tmp, close(fd));
	return ret;
}
