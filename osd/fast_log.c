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

#include "osd/fast_log.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_types.h"
#include "util/macro.h"
#include "util/net.h"
#include "util/string.h"
#include "util/terror.h"

#include <arpa/inet.h>
#include <errno.h>
#include <ev.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PACKED(
struct fast_log_ostor_entry {
	uint16_t ty;
	uint16_t event;
	int32_t error;
	uint64_t cid;
	uint64_t off;
	uint32_t data;
	char pad[4];
});

BUILD_BUG_ON(FLOS_MAX > 0xffff);

void fast_log_ostor(struct fast_log_buf *fb, uint16_t event, uint64_t cid,
		uint64_t off, int32_t error, uint32_t data)
{
	struct fast_log_ostor_entry fe;
	memset(&fe, 0, sizeof(fe));
	fe.ty = FAST_LOG_OSTOR;
	fe.event = event;
	fe.error = error;
	fe.cid = cid;
	fe.off = off;
	fe.data = data;
	fast_log(fb, &fe);
}

static char* flos_err(int32_t error, char *buf, size_t buf_len)
{
	if (error == 0) {
		buf[0] = '\0';
		return buf;
	}
	error = FORCE_POSITIVE(error);
	snprintf(buf, buf_len, "error %d (%s) ", error, terror(error));
	return buf;
}

void fast_log_ostor_dump(struct fast_log_entry *f, char *buf)
{
	struct fast_log_ostor_entry *fe = (struct fast_log_ostor_entry*)f;
	char b[128];
	size_t b_len = sizeof(b);

	switch (fe->event) {
	case FLOS_OCHUNK_EVICT:
		snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"[chunk 0x%"PRIx64"] %sevicting chunk with fd %d\n", fe->cid,
			flos_err(fe->error, b, b_len), fe->data);
		break;
	case FLOS_OCHUNK_WRITE:
		snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"[chunk 0x%"PRIx64"] %swriting %"PRId32 " bytes\n",
			fe->cid, flos_err(fe->error, b, b_len), fe->data);
		break;
	case FLOS_OCHUNK_READ:
		snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"[chunk 0x%"PRIx64"] %sreading %"PRId32 " bytes from offset "
			"%" PRIx64 "\n", fe->cid,
			flos_err(fe->error, b, b_len), fe->data, fe->off);
		break;
	case FLOS_OCHUNK_UNLINK:
		snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"[chunk 0x%"PRIx64"] %sunlinking\n",
			fe->cid, flos_err(fe->error, b, b_len));
		break;
	case FLOS_OCHUNK_WAIT:
		if (fe->error == -EBUSY) {
			snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"[chunk 0x%"PRIx64"] waiting to open busy chunk\n",
				fe->cid);
		}
		else if (fe->error == -EMFILE) {
			snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"[chunk 0x%"PRIx64"] waiting for more file "
				"descriptors to become open available. "
				"num_open = %d\n", fe->cid, fe->data);
		}
		else {
			snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"[chunk 0x%"PRIx64"] unknown FLOS_OCHUNK_WAIT %d",
				fe->cid, fe->error);
		}
		break;
	case FLOS_OCHUNK_ALLOC:
		snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"[chunk 0x%"PRIx64"] %sallocating chunk with fd %d\n",
			fe->cid, flos_err(fe->error, b, b_len), fe->data);
		break;
	case FLOS_LRU_SLEEP:
		snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"ostor lru thread sleeping.  need_lru=%d\n", fe->data);
		break;
	case FLOS_LRU_WAKE:
		snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"ostor lru thread waking.  need_lru=%d\n", fe->data);
		break;
	default:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			 "(unknown ostor event %d)\n", fe->event);
		break;
	}
}
