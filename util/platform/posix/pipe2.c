/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/error.h"
#include "util/platform/pipe2"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/* TODO: add a mutex here ensuring that nobody calls exec in between
 * pipe(2) and fcntl(2) */

int do_pipe(int pipefd[2], enum redfish_plat_flags_t pf)
{
	int ret, curflags, flags;
	ret = pipe(pipefd);
	if (ret) {
		ret = -errno;
		goto error;
	}
	if (pf & WANT_O_NONBLOCK)
		flags |= O_NONBLOCK; 
	if (pf & WANT_O_CLOEXEC)
		flags |= FD_CLOEXEC; 
	if (flags == 0)
		return 0;
	curflags = fcntl(pipefd[PIPE_READ], F_GETFL, 0);
	ret = fcntl(pipefd[PIPE_READ], F_SETFL, flags | curflags);
	if (ret) {
		ret = -errno;
		goto error_close_pipes;
	}
	curflags = fcntl(pipefd[PIPE_WRITE], F_GETFL, 0);
	ret = fcntl(pipefd[PIPE_WRITE], F_SETFL, flags | curflags);
	if (ret) {
		ret = -errno;
		goto error_close_pipes;
	}
	return 0;

error_close_pipes:
	RETRY_ON_EINTR(res, close(pipefd[0]));
	RETRY_ON_EINTR(res, close(pipefd[1]));
error:
	return ret;
}
