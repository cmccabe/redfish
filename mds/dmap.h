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

#ifndef REDFISH_MDS_DMAP_DOT_H
#define REDFISH_MDS_DMAP_DOT_H

#include <stdint.h> /* for uint32_t, etc. */
#include <unistd.h> /* for size_t */

struct dmap;

/*
 * The delegation map has information about which delegations go where.
 *
 * Delegation maps contain no internal locking.
 */

/** Create a new delegation map
 *
 * @return		The delegation map, or an error pointer
 */
extern struct dmap *dmap_alloc(void);

/** Free the delegation map
 *
 * @param dmap		The delegation map
 */
extern void dmap_free(struct dmap *dmap);

/** Look up a delegation ID in the delegation map
 *
 * @param dmap		The delegation map
 *
 * @return		The delegation ID
 */
extern uint64_t dmap_lookup(struct dmap *dmap, const char *path);

/** Add a delegation to the delegation map
 *
 * @param dmap		The delegation map
 * @param path		The full path to add
 * @param dgid		The delegation ID to add
 *
 * @return		0 on success; error code otherwise
 */
extern int dmap_add(struct dmap *dmap, const char *path,
		uint64_t dgid);

/** Remove a delegation from the delegation map
 *
 * @param dmap		The delegation map
 * @param path		The full path to remove
 *
 * @return		0 on success; error code otherwise
 */
extern int dmap_remove(struct dmap *dmap, const char *path);

#endif
