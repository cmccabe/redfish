/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * * This is licensed under the Apache License, Version 2.0.
 */

#ifndef ONEFISH_UTIL_SAFE_IO
#define ONEFISH_UTIL_SAFE_IO

#include "util/compiler.h"

#include <unistd.h>

ssize_t safe_write(int fd, const void *b, size_t c) WARN_UNUSED_RES;
ssize_t safe_pwrite(int fd, const void *b, size_t c, off_t off) WARN_UNUSED_RES;

ssize_t safe_read(int fd, void *b, size_t c) WARN_UNUSED_RES;
ssize_t safe_pread(int fd, void *b, size_t c, off_t off) WARN_UNUSED_RES;
ssize_t safe_read_exact(int fd, void *b, size_t c) WARN_UNUSED_RES;
ssize_t safe_pread_exact(int fd, void *b, size_t c, off_t off) WARN_UNUSED_RES;

int safe_close(int fd);

#endif
