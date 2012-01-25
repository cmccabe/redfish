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

#ifndef REDFISH_UTIL_CLUSTER_MAP_DOT_H
#define REDFISH_UTIL_CLUSTER_MAP_DOT_H

#include <stdint.h> /* for uint32_t, etc. */
#include <unistd.h> /* for size_t */

/*
 * The cluster map has information about the cluster as a whole.
 *
 * Cluster maps contain no internal locking.
 */

struct unitaryc;

struct daemon_info {
	/** IPv4 address */
	uint32_t ip;
	/** IPv4 port */
	uint16_t port;
	/** Nonzero if the node is in the cluster */
	int in;
};

struct cmap {
	/** Cluster map epoch */
	uint64_t epoch;
	/** Number of OSDs */
	int num_osd;
	/** array of OSD info, indexed by ID */
	struct daemon_info *oinfo;
	/** Number of MDSes */
	int num_mds;
	/** array of OSD info, indexed by ID */
	struct daemon_info *minfo;
};

/** Create a new cluster map from the configuration file
 *
 * It will have epoch 1.
 * It's assumed that all nodes will get the same configuration file, so epoch 1
 * will be consistent across the cluster.
 *
 * @param conf		The unitary configuration
 * @param err		(out-param) the error buffer
 * @param err_len	length of the error buffer
 *
 * @return		New cluster map on success, NULL otherwise
 */
extern struct cmap *cmap_from_conf(const struct unitaryc *conf,
			char *err, size_t err_len);

/** Create a new cluster map from a byte buffer.
 *
 * @param buf		The source buffer
 * @param buf_len	Length of the source buffer
 * @param err		(out-param) the error buffer 
 * @param err_len	Length of the error buffer
 *
 * @return		New cluster map on success, NULL otherwise
 */
extern struct cmap *cmap_from_buffer(const char *buf, size_t buf_len,
				char *err, size_t err_len);

/** Serialize a cluster map to a byte buffer
 *
 * @param cmap		The cluster map
 * @param buf_len	(out-param) length of the returned byte buffer
 *
 * @return		A malloc'ed byte buffer on success, NULL on OOM.
 */
extern char *cmap_to_buffer(const struct cmap *cmap, size_t *buf_len);

/** Serialize a cluster map to a human-readable form
 *
 * @param cmap		The cluster map
 * @param buf		(out-param) output buffer
 * @param buf_len	length of the provided buffer
 *
 * @return		0 on success, -ENAMETOOLONG if there wasn't enough
 *			space.
 */
int cmap_to_str(const struct cmap *cmap, char *buf, size_t buf_len);

/** Free a cluster map
 *
 * @param cmap		The cluster map to free
 */
extern void cmap_free(struct cmap *cmap);

#endif
