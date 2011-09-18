/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, New Dream Network
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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
