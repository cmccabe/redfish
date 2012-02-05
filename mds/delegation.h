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

#ifndef REDFISH_MDS_DELEGATION_DOT_H
#define REDFISH_MDS_DELEGATION_DOT_H

#include <stdint.h> /* for uint64_t, etc. */

#include "mds/limits.h" /* for RF_MAX_REPLICAS */
#include "util/compiler.h" /* for PURE */
#include "util/queue.h" /* for STAILQ_ENTRY */

struct dslot;

struct delegation {
	SLIST_ENTRY(delegation) entry;
	/** delegation ID */
	uint64_t dgid;
	/** primary MDS ID for this delegation */
	uint16_t primary;
	/** replica MDSes for this delegation */
	uint16_t replicas[RF_MAX_REPLICAS];
	/** number of replica MDSes for this delegation */
	uint16_t num_replicas;
};

extern int delegation_compare_dgid(const struct delegation *a,
				const struct delegation *b) PURE;

extern void delegation_free(struct delegation *dg);

#endif
