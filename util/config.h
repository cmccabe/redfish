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
