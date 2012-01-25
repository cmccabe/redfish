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

#include "jorm/json.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <json/json.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char RETSTR_JSON_TEST1[] = "{\n\
	\"key0\":\"val0\",\n\
	\"a\":\{\n\
		\"b\":\"bval\",\n\
		\"c\":\n{\
			\"d\":\"dval\"\n\
		}\n\
	}\n\
}\n";

static int do_test_parse_json_file(void)
{
	int ret;
	struct json_object *jo;
	FILE *fp;
	char tempdir[PATH_MAX], path[PATH_MAX], err[512] = { 0 };

	EXPECT_ZERO(get_tempdir(tempdir, PATH_MAX, 0770));
	EXPECT_ZERO(register_tempdir_for_cleanup(tempdir));
	snprintf(path, PATH_MAX, "%s/json1", tempdir);
	fp = fopen(path, "w");
	if (!fp) {
		ret = errno;
		fprintf(stderr, "do_test_parse_json_file: failed to "
			"open %s: error %d\n", path, ret);
		return -EIO;
	}
	fprintf(fp, "%s", RETSTR_JSON_TEST1);
	fclose(fp);
	jo = parse_json_file(path, err, sizeof(err));
	if (!jo) {
		fprintf(stderr, "parse_json_file failed: %s\n", err);
		return -EDOM;
	}
	json_object_put(jo);
	return 0;
}

int main(void)
{
	EXPECT_ZERO(do_test_parse_json_file());
	return EXIT_SUCCESS;
}
