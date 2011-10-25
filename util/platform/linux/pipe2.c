/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/platform/pipe2.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int do_pipe2(int pipefd[2], enum redfish_plat_flags_t pf)
{
	int flags = 0;
	if (pf & WANT_O_CLOEXEC)
		flags |= O_CLOEXEC;
	if (pf & WANT_O_NONBLOCK)
		flags |= O_NONBLOCK;
	if (pipe2(pipefd, flags)) {
		return -errno;
	}
	return 0;
}
