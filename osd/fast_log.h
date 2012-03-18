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

#ifndef REDFISH_OSD_FAST_LOG_DOT_H
#define REDFISH_OSD_FAST_LOG_DOT_H

#include "util/fast_log.h"

#include <stdint.h> /* for uint32_t, etc. */

enum fast_log_osd_event {
	FLOS_OCHUNK_EVICT,
	FLOS_OCHUNK_WRITE,
	FLOS_OCHUNK_READ,
	FLOS_OCHUNK_UNLINK,
	FLOS_OCHUNK_WAIT,
	FLOS_OCHUNK_ALLOC,
	FLOS_LRU_SLEEP,
	FLOS_LRU_WAKE,
	FLOS_MAX,
};

extern void fast_log_ostor(struct fast_log_buf *fb, uint16_t event,
		uint64_t cid, uint64_t off, int32_t error, uint32_t data);

extern void fast_log_ostor_dump(struct fast_log_entry *fe, char *buf);

#endif
