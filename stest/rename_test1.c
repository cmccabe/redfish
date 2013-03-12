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
#include "stest/stest.h"
#include "util/string.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RENAME1_SRCD "/rename1sd"
#define RENAME1_DSTD "/rename1dd"
#define RENAME1_SRCF "/rename1sf"

static void rename_test1_cleanup(struct redfish_client *cli)
{
	redfish_unlink_tree(cli, RENAME1_SRCD);
	redfish_unlink_tree(cli, RENAME1_DSTD);
	redfish_unlink_tree(cli, RENAME1_SRCF);
}

static int rename_test1(struct redfish_client *cli)
{
	struct redfish_file *ofe;
	int ret;

	ret = redfish_mkdirs(cli, 0755, RENAME1_SRCD);
	ST_EXPECT_EQ(ret, 0);
	ST_EXPECT_EQ(stest_stat(cli, RENAME1_SRCD),
		STEST_STAT_RES_DIR);

	/* can't rename root directory */
	ret = redfish_rename(cli, "/", RENAME1_SRCD);
	ST_EXPECT_EQ(ret, -EINVAL);

	/* valid rename */
	ST_EXPECT_EQ(stest_stat(cli, RENAME1_DSTD),
		STEST_STAT_RES_ENOENT);
	ret = redfish_rename(cli, RENAME1_SRCD, RENAME1_DSTD);
	ST_EXPECT_EQ(ret, 0);
	ST_EXPECT_EQ(stest_stat(cli, RENAME1_SRCD),
		STEST_STAT_RES_ENOENT);
	ST_EXPECT_EQ(stest_stat(cli, RENAME1_DSTD),
		STEST_STAT_RES_DIR);

	ret = redfish_create(cli, RENAME1_SRCF, 0644, 0, 0, 0, &ofe);
	ST_EXPECT_EQ(ret, 0);
	ST_EXPECT_ZERO(redfish_close_and_free(ofe));
	ST_EXPECT_EQ(stest_stat(cli, RENAME1_SRCF),
		STEST_STAT_RES_FILE);

	/* can't rename directory over file */
	ret = redfish_rename(cli, RENAME1_DSTD, RENAME1_SRCF);
	printf("ret = %d\n", ret);
	ST_EXPECT_EQ(ret, -ENOTDIR);
	ST_EXPECT_EQ(stest_stat(cli, RENAME1_DSTD),
		STEST_STAT_RES_DIR);
	ST_EXPECT_EQ(stest_stat(cli, RENAME1_SRCF),
		STEST_STAT_RES_FILE);

	return 0;
}

static const char *g_rename_test_usage[] = {
	"rename_test: tests making a bunch of directories.",
	"test-specific environment variables:",
	STEST_REDFISH_CONF_EXPLANATION,
	STEST_REDFISH_USER_EXPLANATION,
	NULL
};

int main(int argc, char **argv)
{
	int ret;
	struct redfish_client *cli = NULL;
	const char *cpath;
	const char *user;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	stest_get_conf_and_user(&cpath, &user);
	stest_init(argc, argv, g_rename_test_usage);
	cli = redfish_connect(cpath, user, redfish_log_to_stderr,
		NULL, err, err_len);
	if (err[0]) {
		stest_add_error("redfish_connect: failed to connect: "
				"%s\n", err);
		return EXIT_FAILURE;
	}

	rename_test1_cleanup(cli);
	ret = rename_test1(cli);
	if (ret == 0)
		rename_test1_cleanup(cli);
	redfish_disconnect_and_release(cli);
	return stest_finish();
}
