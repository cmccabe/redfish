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

#ifndef REDFISH_MSG_MSG_DOT_H
#define REDFISH_MSG_MSG_DOT_H

#include "util/compiler.h"
#include "util/queue.h"
#include "util/tree.h"

#include <stdint.h> /* for uint32_t */
#include <unistd.h> /* for size_t */

/** Represents a message sent or received over the network */
PACKED(
struct msg {
	uint32_t trid;
	uint32_t rem_trid;
	uint32_t len;
	uint16_t ty;
	uint16_t pad;
	char data[0];
});

/** Transactor */
struct mtran {
	union {
		RB_ENTRY(mtran) active_entry;
		STAILQ_ENTRY(mtran) pending_entry;
	} u;
	/* Message to send, if in sending mode; NULL otherwise */
	struct msg *m;
	/* Messenger transactor ID-- used to distinguish between simltaneous
	 * transactions occuring on the same TCP connection */
	uint32_t trid;
	/* Remote messenger transactor ID. The ID this has on the remote end,
	 * or 0 if we don't yet know the remote messenger transactor id.
	 * transactions occuring on the same TCP connection */
	uint32_t rem_trid;
	/* remote IP address */
	uint32_t ip;
	/* remote port */
	uint16_t port;
};

extern void *calloc_msg(uint32_t ty, uint32_t len);

extern struct msg *copy_msg(const struct msg *m);

extern void dump_msg_hdr(struct msg *msg, char *buf, size_t buf_len);

/** A typecast that returns NULL if the message is not long enough
 *
 * This only validates that we can access the fixed-offset fields, of course.
 */
#define MSG_TYPECAST(m, ty) \
	(struct ty*)((unpack_be32(m->len) < sizeof(struct ty)) ? NULL : m)

#endif
