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
