/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MSG_CLIENT_DOT_H
#define REDFISH_MSG_CLIENT_DOT_H

#include "util/compiler.h"

#include <stdint.h>

/* Network messages that can be sent to the client */

enum {
	/** MDS response to a 'create file' request */
	MMM_CREATE_RFILE_RESP = 2000,
	/** MDS response to an 'open file' request */
	MMM_OPEN_RFILE_RESP,
	/** MDS response to a 'list directory' or 'list entries' request */
	MMM_LIST_RESP,
	/** OSD response to a request for a chunk */
	MMM_FETCH_CHUNK_RESP,
};

/* Create file */
PACKED_ALIGNED(8,
struct mmm_create_rfile_resp {
	int32_t rfile;
	uint32_t chunk_addr;
	uint64_t chunk_id;
});
PACKED_ALIGNED(8,
struct mmm_open_rfile_resp {
	int32_t rfile;
	uint32_t chunk_addr;
	uint64_t chunk_id;
});
PACKED_ALIGNED(8,
struct mmm_list_resp {
	/* see net/generic for a description of this format */
	char packed_stat[0];
});
PACKED_ALIGNED(8,
struct mmm_fetch_chunk_resp {
	uint64_t chunk_id;
	uint32_t len;
	char data[0];
});

#endif
