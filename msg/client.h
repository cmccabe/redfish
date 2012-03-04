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

#ifndef REDFISH_MSG_CLIENT_DOT_H
#define REDFISH_MSG_CLIENT_DOT_H

#include "msg/msg.h"
#include "util/compiler.h"

#include <stdint.h>

/* Network messages that can be sent to the client */

enum {
	/** A new chunk ID for the client.  The MDS will send this in
	 * response to a 'create file' request, and also if the client requests
	 * a new chunk ID for a file. */
	MMM_NEW_CHUNK = 2000,
	/** MDS response to an 'open file' request */
	MMM_OPEN_RFILE_RESP,
	/** MDS response to an stat request */
	MMM_STAT_RESP,
	/** MDS response to a 'list directory' or 'list entries' request */
	MMM_LIST_RESP,
	/** OSD response to a request for a chunk */
	MMM_FETCH_CHUNK_RESP,
};

/* Create file */
PACKED(
struct mmm_new_chunk {
	struct msg base;
	uint64_t prid;
	uint32_t chunk_ip;
	uint16_t chunk_port;
});
PACKED(
struct mmm_open_rfile_resp {
	struct msg base;
	int32_t rfile;
	uint32_t chunk_addr;
	uint64_t chunk_id;
});

#define MMM_PACKED_STAT_IS_DIR 0x8000

PACKED(
struct mmm_stat_resp {
	struct msg base;
	char data[0];
	/* struct packed_stat */
});
PACKED(
struct mmm_list_resp {
	struct msg base;
	uint32_t num_elem;
	char data[0];
	/* Packed array of num_elem {
	 * 	pcomp
	 * 	struct packed_stat
	 * }
	 */
});
PACKED(
struct mmm_fetch_chunk_resp {
	struct msg base;
	uint64_t chunk_id;
	uint32_t len;
	char data[0];
});

#endif
