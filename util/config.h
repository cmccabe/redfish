/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_CONFIG_DOT_H
#define REDFISH_UTIL_CONFIG_DOT_H

#include <stdint.h> /* for uint64_t */
#include <unistd.h> /* for size_t */

#define REDFISH_INVAL_FILE_SIZE 0xffffffffffffffffllu

/** Parses a string as a file size.
 *
 * @param str			The string to parse
 * @param out			A buffer where we could write a parse error
 * @param out_len		The length of out_len
 *
 * Returns a number in bytes, or REDFISH_INVAL_FILE_SIZE if parsing failed.
 */
uint64_t parse_file_size(const char *str, char *out, size_t out_len);

#endif
