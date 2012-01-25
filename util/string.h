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

#ifndef REDFISH_UTIL_STRING_H
#define REDFISH_UTIL_STRING_H

#include "util/compiler.h"

#include <stdint.h>  /* for uint32_t* */
#include <stdio.h>  /* for FILE* */
#include <unistd.h> /* for size_t */

/** Returns 1 if str has the given suffix, 0 otherwise.
 *
 * @param str		The input string
 * @param suffix	Suffix to search for
 *
 * @return		1 if str has the given suffix, 0 otherwise.
 */
int has_suffix(const char *str, const char *suffix);

/** Like snprintf, but appends to a string that already exists.
 *
 * @param str		The string to append to
 * @param str_len	Maximum length allowed for str
 * @param fmt		Printf-style format string
 * @param ...		Printf-style arguments
 */
void snappend(char *str, size_t str_len, const char *fmt, ...)
	PRINTF_FORMAT(3, 4);

/** Like snprintf, but returns a nonzero value if there is not enough space.
 *
 * Checking snprintf's return code directly can be inconvenient because it's
 * given in terms of number of characters. This function follows the
 * "nonzero on errors" convention.
 *
 * @param str		The string to append to
 * @param str_len	Maximum length allowed for str
 * @param fmt		Printf-style format string
 * @param ...		Printf-style arguments
 *
 * @returns		0 on success; error code if there is not enough space.
 */
int zsnprintf(char *out, size_t out_len, const char *fmt, ...)
	PRINTF_FORMAT(3, 4);

/** Concatenate an array of strings into a single string.
 *
 * @param lines		A NULL-terminated array of strings
 *
 * @returns		A dynamically allocated string containing all the
 *			strings in lines concatenated together, with newlines
 *			after each one.
 */
char *linearray_to_str(const char **lines);

/** Write a linearray to a file
 *
 * @param file_name	Name of the file to write to. It will be overwritten
 *			if it exists.
 * @param lines		A NULL-terminated array of strings
 * @param err		error output
 * @param err_len	length of error buffer
 */
void write_linearray_to_file(const char *file_name, const char **lines,
				char *err, size_t err_len);

/** Print a series of lines to FILE* fp
 *
 * @param fp		The FILE* to print to
 * @param lines		A NULL-terminated array of lines to print
 */
void print_lines(FILE *fp, const char **lines);

/** Hash a string
 *
 * @param str		the string to hash
 *
 * @return		the string hash
 */
uint32_t ohash_str(const char *str);

/** Convert a byte buffer into a C string.
 *
 * The format is similar to that produced by "od -t x1"
 *
 * @param buf		the buffer
 * @param buf_len	length of the buffer
 * @param str		the output string
 * @param str_len	length of the output string
 */
void hex_dump(const char *buf, size_t buf_len, char *str, size_t str_len);

/** Write a formatted string to the next available position in a
 * fixed-length buffer
 *
 * @param buf		the buffer
 * @param off		(inout) current offset in the buffer
 * @param max		length of the buffer
 * @param fmt		a printf-style format string
 * @param ...		format arguments
 */
void fwdprintf(char *buf, size_t *off, size_t buf_len, const char *fmt, ...)
	PRINTF_FORMAT(4, 5);

#endif
