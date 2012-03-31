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

#include "core/process_ctx.h"
#include "mds/dmap.h"
#include "mds/const.h"
#include "util/error.h"
#include "util/test.h"

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int test_dmap_init_shutdown(void)
{
	struct dmap *dmap;

	dmap = dmap_alloc();
	EXPECT_NOT_ERRPTR(dmap);
	dmap_free(dmap);
	return 0;
}

static int test_dmap_add_remove(void)
{
	struct dmap *dmap;
	uint64_t dgid;

	dmap = dmap_alloc();
	EXPECT_NOT_ERRPTR(dmap);
	dgid = dmap_lookup(dmap, "/a");
	EXPECT_EQ(dgid, RF_ROOT_DGID);
	dgid = dmap_lookup(dmap, "/");
	EXPECT_EQ(dgid, RF_ROOT_DGID);
	EXPECT_EQ(dmap_add(dmap, "/", 123), -EEXIST);
	EXPECT_EQ(dmap_add(dmap, "/a/b", 123), 0);
	EXPECT_EQ(dmap_add(dmap, "/a", 456), 0);
	dgid = dmap_lookup(dmap, "/");
	EXPECT_EQ(dgid, RF_ROOT_DGID);
	dgid = dmap_lookup(dmap, "/a");
	EXPECT_EQ(dgid, 456);
	dgid = dmap_lookup(dmap, "/a/b");
	EXPECT_EQ(dgid, 123);
	EXPECT_EQ(dmap_remove(dmap, "/a"), 0);
	dgid = dmap_lookup(dmap, "/a/b");
	EXPECT_EQ(dgid, 123);
	EXPECT_EQ(dmap_remove(dmap, "/a/b"), 0);
	dgid = dmap_lookup(dmap, "/a/b");
	EXPECT_EQ(dgid, RF_ROOT_DGID);
	EXPECT_EQ(dmap_add(dmap, "/c/d/e", 789), 0);
	EXPECT_EQ(dmap_remove(dmap, "/c/d/e"), 0);
	dgid = dmap_lookup(dmap, "/c/d");
	EXPECT_EQ(dgid, RF_ROOT_DGID);
	dmap_free(dmap);
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	EXPECT_ZERO(utility_ctx_init(argv[0])); 
	EXPECT_ZERO(test_dmap_init_shutdown());
	EXPECT_ZERO(test_dmap_add_remove());
	process_ctx_shutdown();

	return EXIT_SUCCESS;
}
