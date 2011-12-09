/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_SIMPLE_IO_DOT_H
#define REDFISH_SIMPLE_IO_DOT_H

#include "util/compiler.h"

#include <stdio.h> /* for FILE */
#include <unistd.h> /* for ssize_t */

/** Read a whole file using read(2)
 *
 * If the buffer is too small to read the whole file, we'll only read the first
 * part. The bytes we don't read will be zeroed. The last byte will always be
 * zero.
 *
 * @param file	the file name to read
 * @param buf	the buffer to read the file into
 * @param sz	size of the buffer
 *
 * @return	The number of bytes read on success; a negative error code
 *		otherwise.
 */
extern ssize_t simple_io_read_whole_file_zt(const char *file, char *buf,
						int sz);

/** Copy_to_fd failed because of an error on the source fd. */
#define COPY_FD_TO_FD_SRCERR 0x08000000

/** Copy one file descriptor to another, using read(2) and write(2).
 *
 * @param ifd	input file descriptor
 * @param ofd	output file descriptor
 *
 * @return	0 on success; on failure, the error number.
 * 		If the source caused the problem, the error number will be ORed
 * 		with COPY_FD_TO_FD_SRCERR.
 */
extern int copy_fd_to_fd(int ifd, int ofd);

/** Thin wrapper around fprintf that returns either 0 or -EIO.
 *
 * @param out	FILE to write to
 * @param fmt	Format string, as in printf
 * @param ...	varargs (see man page for fprintf)
 *
 * @return	0 on success; a negative error code on I/O error
 */
extern int zfprintf(FILE *out, const char *fmt, ...) PRINTF_FORMAT(2, 3);

#endif
