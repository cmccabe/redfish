/*
 * Copyright 2011-2012 the RedFish authors
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

#ifndef REDFISH_MSG_OSD_DOT_H
#define REDFISH_MSG_OSD_DOT_H

#include "util/compiler.h"

#include <stdint.h>

enum {
	/** Client request to fetch a chunk from the OSD */
	MMM_FETCH_CHUNK_REQ = 4000,
	/** Client request to write a chunk to the OSD */
	MMM_PUT_CHUNK_REQ,
};

PACKED(
struct mmm_fetch_chunk_req {
	uint64_t chunk_id;
	uint32_t start;
	uint32_t len;
});
PACKED(
struct mmm_put_chunk_req {
	uint64_t chunk_id;
	uint32_t len;
	char data[0];
});

#endif
