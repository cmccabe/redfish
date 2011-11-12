/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MSG_CLIENT_DOT_H
#define REDFISH_MSG_CLIENT_DOT_H

#include "msg/msg.h"
#include "util/compiler.h"

#include <stdint.h>

/* Network messages that can be sent to the client */

enum {
	/** A new partition ID for the client.  The MDS will send this in
	 * response to a 'create file' request, and also if the client requests
	 * a new partition ID for a file. */
	MMM_NEW_PARTITION = 2000,
	/** MDS response to an 'open file' request */
	MMM_OPEN_RFILE_RESP,
	/** MDS response to a 'list directory' or 'list entries' request */
	MMM_LIST_RESP,
	/** OSD response to a request for a chunk */
	MMM_FETCH_CHUNK_RESP,
};

/* Create file */
PACKED(
struct mmm_new_partition{
	struct msg base;
	uint64_t prid;
	uint32_t part_ip;
	uint16_t part_port;
});
PACKED(
struct mmm_open_rfile_resp {
	struct msg base;
	int32_t rfile;
	uint32_t chunk_addr;
	uint64_t chunk_id;
});
PACKED(
struct mmm_list_resp {
	struct msg base;
	/* see net/generic for a description of this format */
	char packed_stat[0];
});
PACKED(
struct mmm_fetch_chunk_resp {
	struct msg base;
	uint64_t chunk_id;
	uint32_t len;
	char data[0];
});

#endif
