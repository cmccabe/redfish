/*
 * Copyright 2011-2012 the RedFish authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
