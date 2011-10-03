/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_STR_TO_INT_DOT_H
#define REDFISH_UTIL_STR_TO_INT_DOT_H

#include <unistd.h> /* for size_t */

/** Converts a string to an int, checking for errors.
 *
 * @param str		The input string
 * @param base		Base to use (usually 10)
 * @param i		(out param) the integer
 * @param err		(out param) the error buffer
 * @param err_len	length of err
 */
extern void str_to_int(const char *str, int base, int *i,
		       char *err, size_t err_len);

/** Converts a string to a long long, checking for errors.
 *
 * @param str		The input string
 * @param base		Base to use (usually 10)
 * @param ll		(out param) the long long
 * @param err		(out param) the error buffer
 * @param err_len	length of err
 */
extern void str_to_long_long(const char *str, int base, long long *ll,
		      char *err, size_t err_len);

#endif
