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

#include "util/config.h"
#include "util/simple_io.h"

#include <errno.h>
#include <inttypes.h>
#include <json/json.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_JSON_FILE_SZ 5242880LL

uint64_t parse_file_size(const char *str, char *out, size_t out_len)
{
	const char *suffix;
	int num_len;

	memset(out, 0, out_len);
	if (str[0] == '\0')
		return 0;
	num_len = strspn(str, "0123456789");
	if (num_len == 0) {
		snprintf(out, out_len, "A file size must begin with a number.");
		return REDFISH_INVAL_FILE_SIZE;
	}
	suffix = str + num_len;
	if (suffix[0] == '\0') {
		return atoll(str);
	}
	else if (strcmp("KB", suffix) == 0) {
		return atoll(str) * 1024llu;
	}
	else if (strcmp("MB", suffix) == 0) {
		return atoll(str) * 1024llu * 1024llu;
	}
	else if (strcmp("GB", suffix) == 0) {
		return atoll(str) * 1024llu * 1024llu * 1024llu;
	}
	snprintf(out, out_len, "Unrecognized file size suffix '%s'. "
		 "Valid suffixes are KB, MB, and GB.", suffix);
	return REDFISH_INVAL_FILE_SIZE;
}
