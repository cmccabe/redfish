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

#include "util/platform/socket.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int do_socket(int domain, int type, int proto, enum redfish_plat_flags_t pf)
{
	int res, ret;
	fd = socket(domain, type, proto);
	if (fd < 0) {
		return -errno;
	}
	/* todo: mutex here */
	if (pf & WANT_O_CLOEXEC) {
		int flags = fcntl(fd, F_GETFD);
		flags |= FD_CLOEXEC;
		fcntl(fd, F_SETFD, flags);
	}
	if (pf & WANT_O_NONBLOCK) {
		int on = 1;
		ret = ioctl(fd, FIONBIO, (char*)&on);
		if (ret < 0) {
			RETRY_ON_EINTR(res, close(fd));
			return ret;
		}
	}
#if SO_REUSEADDR_HACK
	{
		int optval = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
			   &optval, sizeof(optval));
	}
#endif
	if (pf & WANT_TCP_NODELAY) {
		int optval = 1;
		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
			   &optval, sizeof(optval));
	}
	return fd;
}

int do_accept(int sock, struct sockaddr *addr, socklen_t len,
		int flags)
{
	int ret;
	socklen_t olen;

	olen = len;
	fd = accept(sock, addr, &olen);
	if (fd < 0) {
		return -errno;
	}
	if (olen > len) {
		int res;
		RETRY_ON_EINTR(res, close(fd));
		return -ENOBUFS;
	}
	/* todo: mutex here */
	if (flags & WANT_O_CLOEXEC) {
		int flags = fcntl(fd, F_GETFD);
		flags |= FD_CLOEXEC;
		fcntl(fd, F_SETFD, flags);
	}
	if (flags & WANT_O_NONBLOCK) {
		int on = 1;
		ret = ioctl(fd, FIONBIO, (char*)&on);
		if (ret < 0) {
			RETRY_ON_EINTR(res, close(fd));
			return ret;
		}
	}
	if (flags & WANT_TCP_NODELAY) {
		int optval = 1;
		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
			   &optval, sizeof(optval));
	}
	return fd;
}
