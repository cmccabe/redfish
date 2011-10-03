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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int do_socket(int domain, int type, int proto, int flags)
{
	int fd;

	if (flags & WANT_O_CLOEXEC) {
		type |= SOCK_CLOEXEC;
	}
	fd = socket(domain, type, proto);
	if (fd < 0) {
		return -errno;
	}
#ifdef REDFISH_SO_REUSEADDR_HACK
	{
		int optval = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
			   &optval, sizeof(optval));
	}
#endif
	if (flags & WANT_TCP_NODELAY) {
		int optval = 1;
		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
			   &optval, sizeof(optval));
	}
	return fd;
}
