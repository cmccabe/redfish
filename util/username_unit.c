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

#include "util/run_cmd.h"
#include "util/test.h"
#include "util/username.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	char buf[512], buf2[512], *newline;
	const char *cvec[] = { "whoami", NULL };

	EXPECT_EQ(get_current_username(buf, 0), -ENAMETOOLONG);
	EXPECT_ZERO(get_current_username(buf, sizeof(buf)));
	EXPECT_ZERO(run_cmd_get_output(buf2, sizeof(buf2), cvec));
	newline = index(buf2, '\n');
	if (newline)
		*newline = '\0';
	if (strcmp(buf, buf2)) {
		fprintf(stderr, "get_current_username returned '%s', but "
			"whoami returned '%s'\n", buf, buf2);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
