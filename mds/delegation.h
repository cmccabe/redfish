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

#ifndef REDFISH_MDS_DELEGATION_DOT_H
#define REDFISH_MDS_DELEGATION_DOT_H

#include <stdint.h> /* for uint64_t, etc. */
#include <time.h> /* for time_t, etc. */

#include "util/compiler.h" /* for PURE */
#include "util/queue.h"
#include "util/tree.h" /* for RB_ENTRY */

struct dslot;

/** Delegation MDS info */
struct dg_mds_info {
	RB_ENTRY(dg_mds_info) entry;
	/** time when we last sent a message to this MDS */
	time_t send_time;
	/** time when we last received a message from this MDS */
	time_t recv_time;
	/** MDS address */
	uint32_t addr;
	/** MDS port */
	uint16_t port;
	/** MDS metadata server ID.  RF_INVAL_MID if this slot is not in use. */
	uint16_t mid;
};

extern int compare_dg_mds_info(const struct dg_mds_info *a,
		const struct dg_mds_info *b) PURE;

RB_HEAD(replicas, dg_mds_info);
RB_PROTOTYPE(replicas, dg_mds_info, entry, compare_dg_mds_info);

struct delegation {
	SLIST_ENTRY(delegation) entry;
	/** delegation ID */
	uint64_t dgid;
	/** Primary MDS ID for this delegation */
	struct dg_mds_info *primary;
	/** All replicas for this delegation (including the primary) */
	struct replicas replica_head;
};

/** Allocate a new delegation
 *
 * @param dgid		The delegation ID
 *
 * @return		The delegation
 */
extern struct delegation *delegation_alloc(uint64_t dgid);

/** Allocate information for a metadata server in this delegation
 *
 * @param dg		The delegation
 * @param mid		The metadata server ID to look up
 * @param is_primary	(out param) 1 if the MDS is primary; 0 otherwise
 *
 * @return		the MDS info, or an error pointer
 */
extern struct dg_mds_info *delegation_alloc_mds(struct delegation *dg,
			uint16_t mid, int is_primary);

/** Lookup the metadata server information that is cached in this MDS
 *
 * @param dg		The delegation
 * @param mid		The metadata server ID to look up
 *
 * @return		the MDS info, or an error pointer
 */
extern struct dg_mds_info *delegation_lookup_mds(struct delegation *dg,
			uint16_t mid);

/** Free the memory associated with a delegation
 *
 * @param dg		The delegation
 */
extern void delegation_free(struct delegation *dg);

#endif
