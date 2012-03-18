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

#ifndef REDFISH_MSG_OSD_DOT_H
#define REDFISH_MSG_OSD_DOT_H

#include "util/compiler.h"

#include <stdint.h>

#define MMM_HFLUSH_FLAG_SYNC 0x1

#define MMM_OSD_MAX_IO_SIZE 0x7fffffff

enum {
	/** Request to read from the OSD */
	MMM_OSD_READ_REQ = 4000,
	/** Client request to write a chunk to the OSD */
	MMM_OSD_HFLUSH_REQ,
};

PACKED(
struct mmm_osd_read_req {
	uint64_t cid;
	uint64_t start;
	uint32_t len;
});
PACKED(
struct mmm_osd_hflush_req {
	struct msg base;
	uint64_t cid;
	uint8_t flags;
	char data[0];
	/* data */
});

#endif
