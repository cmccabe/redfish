/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_UTIL_PLATFORM_PIPE2_DOT_H
#define ONEFISH_UTIL_PLATFORM_PIPE2_DOT_H

/** In the array returned by pipe/pipe2, the read end of the pipe */
#define PIPE_READ 0

/** In the array returned by pipe/pipe2, the write end of the pipe */
#define PIPE_WRITE 1

int do_pipe2(int pipefd[2], int flags);

#endif
