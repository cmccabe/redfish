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

#include "client/fishc.h"
#include "util/test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	const char *version_str;
	struct redfish_version version;

	version = redfish_get_version();
	EXPECT_GT(version.major, 0);
	version_str = redfish_get_version_str();
	if (strspn(version_str, "0123456789.") != strlen(version_str)) {
		fprintf(stderr, "unexpected characters in version string!  "
			"str = '%s'\n", version_str);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
