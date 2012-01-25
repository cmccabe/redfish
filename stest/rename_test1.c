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
#include "mds/limits.h"
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

	rename_test1(cli);
	redfish_disconnect_and_release(cli);
	return stest_finish();
}
