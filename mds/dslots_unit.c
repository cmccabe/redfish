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
#include "mds/delegation.h"
#include "mds/dslots.h"
#include "util/error.h"
#include "util/test.h"

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DSLOTS_UNIT_NUM_DSLOTS 150

static int test_dslots_init_shutdown(void)
{
	struct dslots *dslots;

	dslots = dslots_init(DSLOTS_UNIT_NUM_DSLOTS);
	EXPECT_NOT_ERRPTR(dslots);
	dslots_free(dslots);
	return 0;
}

static struct delegation **make_some_delegations(int num_dgs)
{
	struct delegation **dgs;
	struct delegation *dg;
	int i;

	dgs = calloc(num_dgs, sizeof(struct delegation *));
	if (!dgs)
		return ERR_PTR(ENOMEM);
	for (i = 0; i < num_dgs; ++i) {
		dg = delegation_alloc(i);
		if (IS_ERR(dg))
			goto error;
		dgs[i] = dg;
	}
	return dgs;

error:
	for (; i > 0; --i) {
		free(dgs[i - 1]);
	}
	free(dgs);
	return ERR_PTR(ENOMEM);
}

static uint64_t *dgs_to_dgids(struct delegation **dgs, int num_dgs)
{
	int i;
	uint64_t *dgids;

	dgids = calloc(num_dgs, sizeof(uint64_t));
	if (!dgids)
		return ERR_PTR(ENOMEM);
	for (i = 0; i < num_dgs; ++i) {
		dgids[i] = dgs[i]->dgid;
	}
	return dgids;
}

#define DSLOTS_UNIT_NUM_DGS 1000

static int test_dslots_add_remove(void)
{
	int i;
	struct delegation **dgs;
	struct dslots *dslots;
	struct delegation *dg;
	uint64_t *dgids;

	dslots = dslots_init(DSLOTS_UNIT_NUM_DSLOTS);
	EXPECT_NOT_ERRPTR(dslots);
	dgs = make_some_delegations(DSLOTS_UNIT_NUM_DGS);
	EXPECT_NOT_ERRPTR(dgs);
	EXPECT_ZERO(dslots_add(dslots, dgs, DSLOTS_UNIT_NUM_DGS));
	dgids = dgs_to_dgids(dgs, DSLOTS_UNIT_NUM_DGS);
	EXPECT_NOT_ERRPTR(dgids);
	for (i = 0; i < DSLOTS_UNIT_NUM_DGS; ++i) {
		dg = dslots_lock(dslots, i);
		EXPECT_NOT_ERRPTR(dg);
		dslots_unlock(dslots, dg);
	}
	EXPECT_EQ(dslots_remove(dslots, dgids, DSLOTS_UNIT_NUM_DGS),
			DSLOTS_UNIT_NUM_DGS);
	for (i = 0; i < DSLOTS_UNIT_NUM_DGS; ++i) {
		dg = dslots_lock(dslots, i);
		EXPECT_EQ(dg, NULL);
	}
	free(dgids);
	dslots_free(dslots);
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	EXPECT_ZERO(utility_ctx_init(argv[0])); 
	EXPECT_ZERO(test_dslots_init_shutdown());
	EXPECT_ZERO(test_dslots_add_remove());
	process_ctx_shutdown();

	return EXIT_SUCCESS;
}
