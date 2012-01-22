/*
 * Copyright 2012 the RedFish authors
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

static int mkdirs_test(struct redfish_client *cli)
{
	struct redfish_stat osa;

	/* Very simple case-- only creating one directory in the root */
	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/solo"));
	ST_EXPECT_ZERO(redfish_get_path_status(cli, "/solo", &osa));
	ST_EXPECT_EQUAL(osa.is_dir, 1);
	redfish_free_path_status(&osa);
	stest_set_status(20);
	/* Create a lot of nested directories */
	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/a/b/c/d/e"));
	ST_EXPECT_ZERO(redfish_get_path_status(cli, "/a/b/c/d/e", &osa));
	ST_EXPECT_EQUAL(osa.is_dir, 1);
	redfish_free_path_status(&osa);
	ST_EXPECT_ZERO(redfish_get_path_status(cli, "/a/b/c/d", &osa));
	ST_EXPECT_EQUAL(osa.is_dir, 1);
	redfish_free_path_status(&osa);
	ST_EXPECT_ZERO(redfish_get_path_status(cli, "/a/b/c", &osa));
	ST_EXPECT_EQUAL(osa.is_dir, 1);
	redfish_free_path_status(&osa);
	ST_EXPECT_ZERO(redfish_get_path_status(cli, "/a/b", &osa));
	ST_EXPECT_EQUAL(osa.is_dir, 1);
	redfish_free_path_status(&osa);
	ST_EXPECT_ZERO(redfish_get_path_status(cli, "/a", &osa));
	ST_EXPECT_EQUAL(osa.is_dir, 1);
	redfish_free_path_status(&osa);
	stest_set_status(50);
	/** We should be able to create directories when some of the path
	 * components already exist. */ 
	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/a/b/foo/bar"));
	ST_EXPECT_ZERO(redfish_get_path_status(cli, "/a/b/foo/bar", &osa));
	ST_EXPECT_EQUAL(osa.is_dir, 1);
	redfish_free_path_status(&osa);
	ST_EXPECT_ZERO(redfish_get_path_status(cli, "/a/b/foo", &osa));
	ST_EXPECT_EQUAL(osa.is_dir, 1);
	redfish_free_path_status(&osa);
	stest_set_status(70);
	/** There is no error if all of the directories already exist */
	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/"));
	ST_EXPECT_ZERO(redfish_mkdirs(cli, 0644, "/a"));
	stest_set_status(100);

	return 0;
}

int main(int argc, char **argv)
{
	int ret;
	struct redfish_client *cli = NULL;
	struct redfish_mds_locator **mlocs;
	const char *user;

	stest_init(argc, argv, NULL, 0, &user, &mlocs);
	ret = redfish_connect(mlocs, user, &cli);
	if (ret) {
		stest_add_error("redfish_connect: failed to connect: "
				"error %d\n", ret);
		stest_mlocs_free(mlocs);
		return EXIT_FAILURE;
	}
	stest_mlocs_free(mlocs);

	mkdirs_test(cli);

	redfish_disconnect(cli);
	return stest_finish();
}
