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

/*
 * There is no locking in this file.
 * All operations should be re-entrant!
 */

#include "osd/io.h"
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

static char g_prefix[PATH_MAX] = { 0 };

#define PATH_SUFFIX_SZ (sizeof("01/23456789abcdef"))

int onechunk_set_prefix(const char *prefix)
{
	if (strlen(prefix) + PATH_SUFFIX_SZ + 2 > PATH_MAX) {
		return -ENAMETOOLONG;
	}
	snprintf(g_prefix, PATH_MAX, "%s", prefix);
	return 0;
}

int onechunk_write(uint64_t bid, const void *data, int count, int offset)
{
	int ret, fd, tmp;
	char path[PATH_MAX];
	while (1) {
		snprintf(path, PATH_MAX, "%s/%02x/%014" PRIx64, g_prefix,
			 (int)(bid & 0xff), (bid >> 16));
		fd = open(path, O_APPEND | O_WRONLY | O_CREAT, 0660);
		if (fd != -1)
			break;
		ret = errno;
		if (ret != ENOENT)
			return ret;
		snprintf(path, PATH_MAX, "%s/%02x", g_prefix,
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

int onechunk_read(uint64_t bid, void *data, int count, int offset)
{
	int ret, fd, tmp;
	char path[PATH_MAX];
	snprintf(path, PATH_MAX, "%s/%02x/%014" PRIx64, g_prefix,
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
