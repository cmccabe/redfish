/*
 * Copyright 2011-2012 the Redfish authors
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

#include <stdint.h> /* for uint64_t, etc */
#include <unistd.h> /* for size_t */

/** Converts a string to a uint64_t, checking for errors.
 *
 * The base will be inferred.
 *
 * @param str		The input string
 * @param err		(out param) the error buffer
 * @param err_len	length of err
 *
 * @return		The integer
 */
extern uint64_t str_to_u64(const char *str, char *err, size_t err_len);

/** Converts a string to an int64_t, checking for errors.
 *
 * The base will be inferred.
 *
 * @param str		The input string
 * @param err		(out param) the error buffer
 * @param err_len	length of err
 *
 * @return		The integer
 */
extern int64_t str_to_s64(const char *str, char *err, size_t err_len);

/** Converts a string to an int.
 * The string will be interpreted as an octal integer.
 *
 * @param str		The input string
 * @param err		(out param) the error buffer
 * @param err_len	length of err
 *
 * @return		The integer
 */
extern int str_to_oct(const char *str, char *err, size_t err_len);

/** Converts a string to an int, checking for errors.
 *
 * @param str		The input string
 * @param err		(out param) the error buffer
 * @param err_len	length of err
 *
 * @return		The integer
 */
extern int str_to_int(const char *str, char *err, size_t err_len);

#endif
