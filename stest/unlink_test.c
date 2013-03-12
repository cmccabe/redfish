/*
 * vim: ts=8:sw=8:tw=79:noet
 * 
 * Copyright 2012 the Redfish authors
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
#include "core/env.h"
#include "mds/const.h"
#include "msg/types.h"
#include "stest/stest.h"
#include "util/string.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UNLINK_TEST_NFILES 10

static int unlink_test(struct redfish_client *cli, const char *dirs, int ndirs)
{
	int i, j, ret;
	struct redfish_stat osa;
	struct redfish_file *ofe;
	const char *d;
	char path[RF_PATH_MAX];

	d = dirs;
	for (i = 0; i < ndirs; ++i) {
		/* Create a bunch of files... */
		for (j = 0; j < UNLINK_TEST_NFILES; ++j) {
			ST_EXPECT_ZERO(zsnprintf(path, RF_PATH_MAX,
				"%s/%d", d, j));
			ST_EXPECT_ZERO(redfish_create(cli, path,
				0644, 0, 0, 0, &ofe));
			ST_EXPECT_ZERO(redfish_close_and_free(ofe));
			ST_EXPECT_EQ(redfish_get_path_status(cli,
				path, &osa), 0);
			redfish_free_path_status(&osa);
		}
		/* And unlink them. */
		for (j = 0; j < UNLINK_TEST_NFILES; ++j) {
			ST_EXPECT_ZERO(zsnprintf(path, RF_PATH_MAX,
				"%s/%d", d, j));
			ST_EXPECT_ZERO(redfish_unlink(cli, path));
		}
		/* Verify that we can't stat them... */
		for (j = 0; j < UNLINK_TEST_NFILES; ++j) {
			ST_EXPECT_ZERO(zsnprintf(path, RF_PATH_MAX,
				"%s/%d", d, j));
			ret = redfish_get_path_status(cli, path, &osa);
			if (ret == 0) {
				redfish_free_path_status(&osa);
				stest_add_error("redfish_free_path_status: "
					"found supposedly unlinked file "
					"'%s'", path);
				return -EEXIST;
			}
			if (ret != -ENOENT) {
				stest_add_error("redfish_free_path_status: "
					"expected -ENOENT, got error %d\n",
					ret);
				return -EIO;
			}
		}
		/* Next... */
		stest_set_status(((i + 1) / ndirs) * 100);
		d += strlen(d);
	}

	return 0;
}

static int process_dirs(char *dirs)
{
	int ndirs = 0;

	while (1) {
		char c = *dirs;

		if (c == '\0') {
			++ndirs;
			break;
		}
		else if (c == ',') {
			++ndirs;
			*dirs = '\0';
		}
		++dirs;
	}
	return ndirs;
}

static const char *g_unlink_test_usage[] = {
	"unlink_test: tests creating and unlinking files and directories.",
	"test-specific environment variables:",
	STEST_REDFISH_CONF_EXPLANATION,
	STEST_REDFISH_USER_EXPLANATION,
	"ST_ROOT: comma-separated list of directories to use.",
	NULL
};

int main(int argc, char **argv)
{
	int ndirs;
	struct redfish_client *cli = NULL;
	const char *cpath;
	const char *user;
	const char *dir_str;
	char *dirs = NULL;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	stest_get_conf_and_user(&cpath, &user);
	dir_str = getenv_with_default("ST_ROOT", "/");
	stest_init(argc, argv, g_unlink_test_usage);
	cli = redfish_connect(cpath, user, redfish_log_to_stderr,
		NULL, err, err_len);
	if (err[0]) {
		stest_add_error("redfish_connect: failed to connect: "
				"%s\n", err);
		return EXIT_FAILURE;
	}

	dirs = strdup(dir_str);
	ndirs = process_dirs(dirs);
	unlink_test(cli, dirs, ndirs);
	free(dirs);
	redfish_disconnect_and_release(cli);
	return stest_finish();
}
