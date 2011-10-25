/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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
