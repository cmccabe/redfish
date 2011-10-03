/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/safe_io.h"
#include "util/simple_io.h"

#include <errno.h>
#include <fcntl.h>
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
