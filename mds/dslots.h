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

#ifndef REDFISH_MDS_DSLOTS_DOT_H
#define REDFISH_MDS_DSLOTS_DOT_H

#include <stdint.h> /* for uint64_t, etc. */

struct delegation;
struct dslots;

/** Create a bank of dslots
 *
 * @param num_dslots	Number of dslots to allocate
 *
 * @return		The bank of dslots, or an error pointer on failure.
 */
extern struct dslots *dslots_init(int num_dslots);

/** Add new delegation(s)
 *
 * @param dslots	The bank of dslots
 * @param dg		Array of pointers to delegations to add
 * @param dg_len	Number of delegations to add
 *
 * @return		0 on success; error code otherwise
 */
extern int dslots_add(struct dslots *dslots, struct delegation **dgs,
		int num_dgs);

/** Remove delegation(s)
 *
 * @param dslots	The bank of dslots
 * @param dgids		Array of delegation IDs to delete
 * @param dg_len	Number of delegations to delete
 *
 * @return		The number of delegations removed
 */
extern int dslots_remove(struct dslots *dslots, uint64_t *dgids, int num_dgs);

/** Lock and retrieve a delegation
 *
 * @param dslots	The bank of dslots
 * @param dgid		The delegation ID
 *
 * @return		The delegation, or NULL if the delegation ID no longer
 *			exists.
 */
extern struct delegation *dslots_lock(struct dslots *dslots, uint64_t dgid);

/** Unlock a delegation
 *
 * @param dslots	The bank of dslots
 * @param dg		The delegation to unlock
 */
extern void dslots_unlock(struct dslots *dslots, const struct delegation *dg);

/** Free a bank of dslots
 *
 * @param dslots		The bank of dslots
 */
extern void dslots_free(struct dslots *dslots);

#endif
