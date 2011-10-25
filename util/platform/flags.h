/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_PLATFORM_SOCKET_FLAGS_H
#define REDFISH_UTIL_PLATFORM_SOCKET_FLAGS_H

enum redfish_plat_flags_t {
	WANT_O_CLOEXEC = 0x1,
	WANT_TCP_NODELAY = 0x2,
	WANT_O_NONBLOCK = 0x4,
};

#endif
