/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_SIMPLE_IO_DOT_H
#define ONEFISH_SIMPLE_IO_DOT_H

#include <unistd.h> /* for ssize_t */

/** Read a whole file using read(2)
 *
 * If the buffer is too small to read the whole file, we'll only read the first
 * part. The bytes we don't read will be zeroed. The last byte will always be
 * zero.
 *
 * file: the file name to read
 *
 * Returns the number of bytes read on success; error code otherwise.
 */
ssize_t simple_io_read_whole_file(const char *file, char *buf, int sz);

#endif
