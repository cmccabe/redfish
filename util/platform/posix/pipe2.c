/*
 * Copyright 2011-2012 the RedFish authors
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
