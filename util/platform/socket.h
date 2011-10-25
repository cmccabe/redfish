/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_PLATFORM_SOCKET_DOT_H
#define REDFISH_UTIL_PLATFORM_SOCKET_DOT_H

#include "util/platform/flags.h"

#include <netinet/in.h>
#include <sys/socket.h>


/** Similar to the traditional POSIX socket(2) call, but hides some
 * platform-specific stuff.
 *
 * @param domain	socket domain
 * @param type		socket type
 * @param proto		socket proto
 * @param flags		redfish platform flags
 *
 * @return		A socket on success; or a negative error code on error.
 */
extern int do_socket(int domain, int type, int proto,
		enum redfish_plat_flags_t pf);

/** Similar to the traditional POSIX accept(2) call, but hides some
 * platform-specific stuff.
 *
 * @param sock		listening socket
 * @param addr		(out param) remote address
 * @param addr_len	Length of 'addr'. Unlike the POSIX function, this is
 *			 just an input, not an inout parameter. If you give a
 *			 buffer that is too short, we'll return -ENOBUFS. Giving
 *			 a correctly sized buffer should be as simple as
 *			 passing sizeof(addr).
 * @param flags		redfish platform flags
 *
 * @return		A socket on success; or a negative error code on error.
 */
extern int do_accept(int sock, struct sockaddr *addr, socklen_t len,
		enum redfish_plat_flags_t pf);

#endif
