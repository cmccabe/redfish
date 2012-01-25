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

#include "core/process_ctx.h"
#include "mds/user.h"
#include "util/error.h"
#include "util/string.h"
#include "util/test.h"

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int test_alloc_free(void)
{
	struct udata *udata;

	udata = udata_create_default();
	EXPECT_NOT_ERRPTR(udata);
	udata_free(udata);
	return 0;
}

static int do_test_lookups(struct udata *udata)
{
	const struct user *u;
	const struct group *g;
	
	g = udata_lookup_group(udata, RF_SUPERUSER_NAME);
	EXPECT_NOT_ERRPTR(g);
	EXPECT_ZERO(strcmp(g->name, RF_SUPERUSER_NAME));
	g = udata_lookup_group(udata, RF_NOBODY_NAME);
	EXPECT_NOT_ERRPTR(g);
	EXPECT_ZERO(strcmp(g->name, RF_NOBODY_NAME));
	u = udata_lookup_user(udata, RF_SUPERUSER_NAME);
	EXPECT_NOT_ERRPTR(u);
	EXPECT_ZERO(strcmp(u->name, RF_SUPERUSER_NAME));
	EXPECT_EQ(user_in_gid(u, RF_SUPERUSER_GID), 1);
	return 0;
}

static int test_lookups(void)
{
	struct udata *udata;
	struct user *u;

	udata = udata_create_default();
	EXPECT_NOT_ERRPTR(udata);
	EXPECT_ZERO(do_test_lookups(udata));
	u = udata_lookup_user(udata, RF_NOBODY_NAME);
	if (IS_ERR(u))
		return PTR_ERR(u);
	EXPECT_EQ(user_in_gid(u, RF_SUPERUSER_GID), 0);
	EXPECT_ZERO(user_add_segid(udata, RF_NOBODY_NAME, 0));
	u = udata_lookup_user(udata, RF_NOBODY_NAME);
	if (IS_ERR(u))
		return PTR_ERR(u);
	EXPECT_EQ(user_in_gid(u, RF_SUPERUSER_GID), 1);
	udata_free(udata);
	return 0;
}

static int test_pack(void)
{
	uint32_t off, off2;
	char buf[32765];
	struct udata *udata, *udata2;

	udata = udata_create_default();
	EXPECT_NOT_ERRPTR(udata);
	EXPECT_ZERO(do_test_lookups(udata));
	memset(buf, 0, sizeof(buf));
	off = 0;
	EXPECT_EQ(pack_udata(udata, buf, &off, 1), -ENAMETOOLONG);
	EXPECT_ZERO(pack_udata(udata, buf, &off, sizeof(buf)));
	off2 = 0;
	udata2 = unpack_udata(buf, &off2, off / 2);
	EXPECT_ERRPTR(udata2);
	udata2 = unpack_udata(buf, &off2, off);
	EXPECT_NOT_ERRPTR(udata2);
	EXPECT_ZERO(do_test_lookups(udata2));
	udata_free(udata);
	udata_free(udata2);
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	EXPECT_ZERO(utility_ctx_init(argv[0])); /* for g_fast_log_mgr */
	EXPECT_ZERO(test_alloc_free());
	EXPECT_ZERO(test_lookups());
	EXPECT_ZERO(test_pack());

	process_ctx_shutdown();

	return EXIT_SUCCESS;
}
