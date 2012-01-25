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

#include "util/config.h"
#include "util/test.h"

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

static int test_parse_file_size(void)
{
	uint64_t res;
	char out[128];
	res = parse_file_size("123", out, sizeof(out));
	if (res != 123llu) {
		return -EINVAL;
	}

	res = parse_file_size("1111111111111", out, sizeof(out));
	if (res != 1111111111111llu) {
		return -EINVAL;
	}

	res = parse_file_size("", out, sizeof(out));
	if (res != 0) {
		return -EINVAL;
	}

	res = parse_file_size("2KB", out, sizeof(out));
	if (res != 2048llu) {
		return -EINVAL;
	}

	res = parse_file_size("5MB", out, sizeof(out));
	if (res != 5242880llu) {
		return -EINVAL;
	}

	res = parse_file_size("16GB", out, sizeof(out));
	if (res != 17179869184llu) {
		return -EINVAL;
	}
	return 0;
}

int main(void)
{
	int ret;
	ret = test_parse_file_size();
	if (ret) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
