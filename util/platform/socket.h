
/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_UTIL_PLATFORM_SOCKET_DOT_H
#define ONEFISH_UTIL_PLATFORM_SOCKET_DOT_H

enum {
	WANT_O_CLOEXEC = 0x1
};

/** Similar to the traditional POSIX socket(2) call, but hides some
 * platform-specific stuff.
 *
 * @param domain	socket domain
 * @param type		socket type
 * @param proto		socket proto
 * @param flags		onefish socket flags
 *
 * @return		A socket on success; or a negative error code on error.
 */
extern int do_socket(int domain, int type, int proto, int flags);

#endif
