/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_PLATFORM_PIPE2_DOT_H
#define REDFISH_UTIL_PLATFORM_PIPE2_DOT_H

#include "util/platform/flags.h"

/** In the array returned by pipe/pipe2, the read end of the pipe */
#define PIPE_READ 0

/** In the array returned by pipe/pipe2, the write end of the pipe */
#define PIPE_WRITE 1

int do_pipe2(int pipefd[2], enum redfish_plat_flags_t flags);

#endif
