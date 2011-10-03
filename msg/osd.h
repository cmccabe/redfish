/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MSG_OSD_DOT_H
#define REDFISH_MSG_OSD_DOT_H

#include "util/compiler.h"

#include <stdint.h>

enum {
	/** Client request to fetch a chunk from the OSD */
	MMM_FETCH_CHUNK_REQ = 3000,
	/** Client request to write a chunk to the OSD */
	MMM_PUT_CHUNK_REQ,
};

PACKED_ALIGNED(8,
struct mmm_fetch_chunk_req {
	uint64_t chunk_id;
	uint32_t start;
	uint32_t len;
});
PACKED_ALIGNED(8,
struct mmm_put_chunk_req {
	uint64_t chunk_id;
	uint32_t len;
	char data[0];
});

#endif
