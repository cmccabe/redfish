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

#include "client/fishc.h"
#include "stest/stest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STEST_FNAME "/test.txt"
#define SAMPLE_DATA "sample data!"
#define SAMPLE_DATA_LEN ((sizeof(SAMPLE_DATA) /  \
			  sizeof(SAMPLE_DATA[0])) - 1)

static int write_then_read(struct redfish_client *cli)
{
	struct redfish_file *ofe;
	char ibuf[SAMPLE_DATA_LEN] = SAMPLE_DATA;
	char obuf[SAMPLE_DATA_LEN] = { 0 };

	ofe = NULL;
	ST_EXPECT_ZERO(redfish_create(cli, STEST_FNAME, 0644, 0, 0, 0, &ofe));
	ST_EXPECT_ZERO(redfish_write(ofe, ibuf, SAMPLE_DATA_LEN));
	ST_EXPECT_ZERO(redfish_close_and_free(ofe));
	ofe = NULL;
	ST_EXPECT_ZERO(redfish_open(cli, STEST_FNAME, &ofe));
	ST_EXPECT_EQ(redfish_read(ofe, obuf, SAMPLE_DATA_LEN),
			SAMPLE_DATA_LEN);
	ST_EXPECT_ZERO(strcmp(ibuf, obuf));
	ST_EXPECT_ZERO(redfish_close_and_free(ofe));

	return 0;
}

static const char *g_write_then_read_test_usage[] = {
	"mkdirs_test: tests making a bunch of directories and listing them.",
	"test-specific environment variables:",
	STEST_REDFISH_CONF_EXPLANATION,
	STEST_REDFISH_USER_EXPLANATION,
	"ST_CRASH: if 1, crash without properly disconnecting.",
	NULL
};

int main(int argc, char **argv)
{
	int ret;
	struct redfish_client *cli = NULL;
	const char *cpath;
	const char *user;
	const char *error;

	stest_get_conf_and_user(&cpath, &user);
	error = getenv("ST_CRASH");
	stest_init(argc, argv, g_write_then_read_test_usage);
	ret = redfish_connect(cpath, user, &cli);
	if (ret) {
		stest_add_error("redfish_connect: failed to connect: "
				"error %d\n", ret);
		return EXIT_FAILURE;
	}

	write_then_read(cli);

	stest_set_status(10);
	if (error)
		_exit(1);
	redfish_disconnect_and_release(cli);
	return stest_finish();
}
