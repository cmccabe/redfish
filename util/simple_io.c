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

#include "util/safe_io.h"
#include "util/simple_io.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

ssize_t simple_io_read_whole_file_zt(const char *fname, char *buf, int sz)
{
	ssize_t res;
	int fd = open(fname, O_RDONLY);
	if (fd < 0) {
		int err = errno;
		return -err;
	}
	memset(buf, 0, sz);
	if (sz == 0)
		return 0;
	res = safe_pread(fd, buf, sz - 1, 0);
	if (res < 0) {
		close(fd);
		return res;
	}
	close(fd);
	return res;
}

int copy_fd_to_fd(int ifd, int ofd)
{
	while (1) {
		ssize_t res, wes;
		char buf[8196];

		res = safe_read(ifd, buf, sizeof(buf));
		if (res < 0)
			return res | COPY_FD_TO_FD_SRCERR;
		else if (res == 0)
			return 0;
		wes = safe_write(ofd, buf, res);
		if (wes < 0)
			return res & (~COPY_FD_TO_FD_SRCERR);
		if (res < (ssize_t)sizeof(buf))
			return 0;
	}
}

int zfprintf(FILE *out, const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vfprintf(out, fmt, ap);
	va_end(ap);

	if (ret < 0)
		return -EIO;
	return 0;
}
