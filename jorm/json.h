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

#ifndef JORM_JSON_DOT_H
#define JORM_JSON_DOT_H

#include <stdint.h>
#include <unistd.h> /* for size_t */

/* json/json.h requires stdint.h to be included before it. */
#include "json/json.h"

#define MAX_JSON_FILE_SZ 5242880

/** Parses a file containing JSON data
 * This reads the whole file into memory. To ensure sanity, files larger than
 * 5 MB are rejected.
 *
 * @param str			the file to read
 * @param err			error output buffer
 * @param err_len		size of error output buffer
 *
 * @return			a JSON object or NULL if parsing failed.
 *				If parsing failed, the err buffer will be set.
 */
struct json_object* parse_json_file(const char *file_name,
				char *err, size_t err_len);

/** Parses a JSON string
 *
 * @param str			the string to parse
 * @param err			error output buffer
 * @param err_len		size of error output buffer
 *
 * @return			a JSON object or NULL if parsing failed.
 *				If parsing failed, the err buffer will be set.
 */
struct json_object* parse_json_string(const char *str,
				char *err, size_t err_len);

/** Return a string corresponding to a JSON type
 *
 * @param ty			The JSON type
 *
 * @return			a statically allocated string
 */
const char *json_ty_to_str(enum json_type ty);

#endif
