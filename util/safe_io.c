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

#define _XOPEN_SOURCE 500

#include "util/safe_io.h"

#include <unistd.h>
#include <errno.h>

ssize_t safe_write(int fd, const void *b, size_t c)
{
	int res;

	while (c > 0) {
		res = write(fd, b, c);
		if (res < 0) {
			if (errno != EINTR)
				return -errno;
			continue;
		}
		c -= res;
		b = (char *)b + res;
	}
	return 0;
}

ssize_t safe_pwrite(int fd, const void *b, size_t c, off_t off)
{
	int res;

	while (c > 0) {
		res = pwrite(fd, b, c, off);
		if (res < 0) {
			if (errno != EINTR)
				return -errno;
			continue;
		}
		c -= res;
		b = (char *)b + res;
		off += res;
	}
	return 0;
}

ssize_t safe_read(int fd, void *b, size_t c)
{
	int res;
	size_t cnt = 0;

	while (cnt < c) {
		res = read(fd, b, c - cnt);
		if (res <= 0) {
			if (res == 0)
				return cnt;
			if (errno != EINTR)
				return -errno;
			continue;
		}
		cnt += res;
		b = (char *)b + res;
	}
	return cnt;
}

ssize_t safe_read_exact(int fd, void *b, size_t c)
{
	int ret = safe_read(fd, b, c);
	if (ret < 0)
		return ret;
	if ((size_t)ret != c)
		return -EDOM;
	return 0;
}

ssize_t safe_pread(int fd, void *b, size_t c, off_t off)
{
	size_t cnt = 0;
	int res;
	char *buf = (char*)b;

	while (cnt < c) {
		res = pread(fd, buf + cnt, c - cnt, off + cnt);
		if (res <= 0) {
			if (res == 0)
				return cnt;
			if (errno != EINTR)
				return -errno;
			continue;
		}

		cnt += res;
	}
	return cnt;
}

ssize_t safe_pread_exact(int fd, void *b, size_t c, off_t off)
{
	int ret = safe_pread(fd, b, c, off);
	if (ret < 0)
		return ret;
	if (ret != (int)c)
		return -EDOM;
	return 0;
}

int safe_close(int fd)
{
	while (close(fd) == -1) {
		if (errno != EINTR)
			return -errno;
	}
	return 0;
}
