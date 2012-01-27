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

#include "util/compiler.h"
#include "util/run_cmd.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	char st_trivial[PATH_MAX];

	EXPECT_ZERO(get_colocated_path(argv[0], "st_trivial",
			      st_trivial, sizeof(st_trivial)));
	EXPECT_ZERO(run_cmd(st_trivial, "-h", (char*)NULL));
	EXPECT_ZERO(run_cmd(st_trivial, "-c", "/tmp/redfish_conf",
			"-f", (char*)NULL));
	EXPECT_ZERO(run_cmd(st_trivial, "-c", "/tmp/redfish_conf",
			"-f", "error=0", (char*)NULL));
	EXPECT_NONZERO(run_cmd(st_trivial, "-c", "/tmp/redfish_conf",
			"-f", "error=1", (char*)NULL));
	EXPECT_ZERO(run_cmd(st_trivial, "-c", "/tmp/redfish_conf",
			"-u", "myuser", "-f", "error=0", (char*)NULL));
	return 0;
}
