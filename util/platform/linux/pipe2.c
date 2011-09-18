/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/platform/pipe2.h"

#include <errno.h>
#include <unistd.h>

int do_pipe2(int pipefd[2], int flags)
{
	if (pipe2(pipefd, flags)) {
		return -errno;
	}
	return 0;
}
