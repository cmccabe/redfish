/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_OSD_NET_DOT_H
#define ONEFISH_OSD_NET_DOT_H

#include "util/compiler.h"

#include <stdint.h>

enum {
	MMM_REQUEST_CHUNK_REPLY,
	MMM_REQUEST_CHUNK_NACK,
	MMM_REQUEST_CHUNK,
	MMM_PUT_CHUNK,
	MMM_PUT_CHUNK_REPLY,
};

PACKED_ALIGNED(8,
struct mmm_request_chunk {
	uint64_t chunk_id;
	uint32_t start;
	uint32_t len;
});

PACKED_ALIGNED(8,
struct mmm_request_chunk_nack {
	uint64_t chunk_id;
	uint32_t err;
});

PACKED_ALIGNED(8,
struct mmm_request_chunk_reply {
	uint64_t chunk_id;
	uint32_t len;
	char data[0];
});

PACKED_ALIGNED(8,
struct mmm_put_chunk {
	uint64_t chunk_id;
	uint32_t len;
	char data[0];
});

PACKED_ALIGNED(8,
struct mmm_put_chunk_reply {
	uint64_t chunk_id;
	uint32_t err;
});

struct daemon;

/** Start the object storage daemon main loop
 *
 * @param d		The daemon configuration
 *
 * @return		The return value of the program
 */
extern int osd_main_loop(struct daemon *d);

#endif
