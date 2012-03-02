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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *g_startup_test_usage[] = {
	"startup_test: tests starting the Redfish client",
	"test-specific environment variables:",
	STEST_REDFISH_CONF_EXPLANATION,
	STEST_REDFISH_USER_EXPLANATION,
	"ST_CRASH: set this to 1 to crash before disconnecting the client.",
	NULL
};

int main(int argc, char **argv)
{
	struct redfish_client *cli = NULL;
	const char *user;
	const char *cpath;
	const char *crash;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	stest_get_conf_and_user(&cpath, &user);
	crash = getenv("ST_CRASH");
	stest_init(argc, argv, g_startup_test_usage);
	cli = redfish_connect(cpath, user, redfish_log_to_stderr,
		NULL, err, err_len);
	if (err[0]) {
		stest_add_error("redfish_connect: failed to connect: "
				"%s\n", err);
		return EXIT_FAILURE;
	}

	stest_set_status(10);

	if (crash) {
		_exit(1);
	}
	if (cli) {
		redfish_disconnect_and_release(cli);
	}

	return stest_finish();
}
