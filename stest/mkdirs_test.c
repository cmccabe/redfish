/*
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
#include "util/bitfield.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LISTDIRS_TEST_ENTRIES 128

static int mkdirs_test(struct redfish_client *cli)
{
	/* Very simple case-- only creating one directory in the root */
	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/solo"));
	ST_EXPECT_EQ(stest_stat(cli, "/solo"), STEST_STAT_RES_DIR);
	stest_set_status(20);
	/* Create a lot of nested directories */
	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/a/b/c/d/e"));
	ST_EXPECT_EQ(stest_stat(cli, "/a/b/c/d/e"), STEST_STAT_RES_DIR);
	ST_EXPECT_EQ(stest_stat(cli, "/a/b/c/d"), STEST_STAT_RES_DIR);
	ST_EXPECT_EQ(stest_stat(cli, "/a/b/c"), STEST_STAT_RES_DIR);
	ST_EXPECT_EQ(stest_stat(cli, "/a/b"), STEST_STAT_RES_DIR);
	ST_EXPECT_EQ(stest_stat(cli, "/a"), STEST_STAT_RES_DIR);
	stest_set_status(50);
	/** We should be able to create directories when some of the path
	 * components already exist. */ 
	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/a/b/foo/bar"));
	ST_EXPECT_EQ(stest_stat(cli, "/a/b/foo/bar"), STEST_STAT_RES_DIR);
	ST_EXPECT_EQ(stest_stat(cli, "/a/b/foo"), STEST_STAT_RES_DIR);
	stest_set_status(70);
	/** There is no error if all of the directories already exist */
	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/"));
	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/a"));
	stest_set_status(100);

	return 0;
}

struct listdirs_test_fn1_data {
	BITFIELD_DECL(found, MAX_LISTDIRS_TEST_ENTRIES);
};

static int listdirs_test_fn1(const struct redfish_dir_entry *oda, void *v)
{
	struct listdirs_test_fn1_data *data = v;
	int idx = -1;

	if (sscanf(oda->name, "%d", &idx) != 1)
		return -EINVAL;
	if ((idx < 0) || (idx > MAX_LISTDIRS_TEST_ENTRIES))
		return -EINVAL;
	BITFIELD_SET(data->found, idx);
	return 0;
}

static int listdirs_test(struct redfish_client *cli)
{
	struct listdirs_test_fn1_data data;

	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/mkdirs_test2"));
	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/mkdirs_test2/0"));
	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/mkdirs_test2/1"));
	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/mkdirs_test2/2"));
	memset(&data, 0, sizeof(data));
	ST_EXPECT_EQ(stest_listdir(cli, "/mkdirs_test2", listdirs_test_fn1,
			&data), 3);
	ST_EXPECT_NOT_EQ(BITFIELD_TEST(data.found, 0), 0);
	ST_EXPECT_NOT_EQ(BITFIELD_TEST(data.found, 1), 0);
	ST_EXPECT_NOT_EQ(BITFIELD_TEST(data.found, 2), 0);
	return 0;
}

static const char *g_mkdirs_test_usage[] = {
	"mkdirs_test: tests making a bunch of directories and listing them.",
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

	stest_get_conf_and_user(&cpath, &user);
	stest_init(argc, argv, g_mkdirs_test_usage);
	ret = redfish_connect(cpath, user, &cli);
	if (ret) {
		stest_add_error("redfish_connect: failed to connect: "
				"error %d\n", ret);
		return EXIT_FAILURE;
	}

	if (mkdirs_test(cli))
		goto done;
	if (listdirs_test(cli))
		goto done;
done:
	redfish_disconnect_and_release(cli);
	return stest_finish();
}
