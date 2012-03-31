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

#include "mds/delegation.h"
#include "core/glitch_log.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/queue.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RB_GENERATE(replicas, dg_mds_info, entry, compare_dg_mds_info);

int compare_dg_mds_info(const struct dg_mds_info *a,
		const struct dg_mds_info *b)
{
	if (a->mid < b->mid)
		return -1;
	else if (a->mid > b->mid)
		return 1;
	return 0;
}

struct delegation *delegation_alloc(uint64_t dgid)
{
	struct delegation *dg;

	dg = calloc(1, sizeof(struct delegation));
	if (!dg)
		return ERR_PTR(ENOMEM);
	dg->dgid = dgid;
	RB_INIT(&dg->replica_head);
	return dg;
}

struct dg_mds_info *delegation_alloc_mds(struct delegation *dg,
			uint16_t mid, int is_primary)
{
	struct dg_mds_info *mi;

	if ((is_primary) && (dg->primary))
		return ERR_PTR(EINVAL);
	mi = calloc(1, sizeof(struct dg_mds_info));
	if (!mi)
		return ERR_PTR(ENOMEM);
	if (is_primary)
		dg->primary = mi;
	mi->mid = mid;
	RB_INSERT(replicas, &dg->replica_head, mi);
	return mi;
}

struct dg_mds_info *delegation_lookup_mds(struct delegation *dg, uint16_t mid)
{
	struct dg_mds_info exemplar, *mi;
	memset(&exemplar, 0, sizeof(exemplar));
	exemplar.mid = mid;
	mi = RB_FIND(replicas, &dg->replica_head, &exemplar);
	if (!mi)
		return ERR_PTR(ENOENT);
	return mi;
}

void delegation_free(struct delegation *dg)
{
	struct dg_mds_info *mi, *mi_tmp;

	RB_FOREACH_SAFE(mi, replicas, &dg->replica_head, mi_tmp) {
		RB_REMOVE(replicas, &dg->replica_head, mi);
		free(mi);
	}
	free(dg);
}
