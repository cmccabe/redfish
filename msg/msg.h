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

struct mconn;
struct mtran;

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

/** Transactor callback. This is called whenever a message is sent or received
 * by the messenger.
 *
 * On a receive completed event, tr->state == MTRAN_STATE_RECV.
 * On a send completed event, tr->state == MTRAN_STATE_SENT.
 * See msg/msg.h for a description of the tr->m field.
 *
 * If you don't call mtran_*_next, the messenger doesn't have anything more to
 * do with your tranactor. You should probably either free it with mtran_free,
 * or pass it to some other subsystem that cares about it.
 */
typedef void (*msgr_cb_t)(struct mconn *conn, struct mtran *tr);

enum mtran_state {
	MTRAN_STATE_IDLE = 0,
	/** The mtran is in the 'pending' queue of an mconn.  It is waiting to
	 * be sent, or in the process of being sent. */
	MTRAN_STATE_SENDING = 1,
	/** We just sent the message associated with this mtran */
	MTRAN_STATE_SENT = 2,
	/** This mtran is in the 'active' tree of an mconn.  That means we are
	 * either waiting for a message to come in for this mtran, or a message
	 * is in the process of coming in.
	 */
	MTRAN_STATE_ACTIVE = 3,
	/** We just received a message on this mtran */
	MTRAN_STATE_RECV = 4,
};

/** Transactor */
struct mtran {
	union {
		RB_ENTRY(mtran) active_entry;
		STAILQ_ENTRY(mtran) pending_entry;
	} u;
	/** The message.
	 *
	 * This is an overloaded field (maybe too overloaded?)
	 *
	 * Before mtran_send, this contains the message to send.  Once the
	 * message has been sent, this contains NULL if the send was
	 * successful, or an error pointer if it wasn't.
	 *
	 * In a transactor created by mtran_listen, this contains the message
	 * that was received.
	 *
	 * What if you invoke mtran_recv_next on a transactor?  Well, some time
	 * later the appropriate response callback will be made.  At that point,
	 * this field can have several values.  It will contain
	 * ERR_PTR(ETIMEDOUT) if we timed out waiting for a response.  It will
	 * contain a different error pointer if there was some TCP-level error.
	 * If we successfully received a response, it will contain a pointer to
	 * the response message that was received.
	 */
	struct msg *m;
	/** Callback invoked when a message is sent or received */
	msgr_cb_t cb;
	/** Messenger transactor ID-- used to distinguish between simltaneous
	 * transactions occuring on the same TCP connection */
	uint32_t trid;
	/** Remote messenger transactor ID. The ID this has on the remote end,
	 * or 0 if we don't yet know the remote messenger transactor id.
	 * transactions occuring on the same TCP connection */
	uint32_t rem_trid;
	/** remote IP address */
	uint32_t ip;
	/** remote port */
	uint16_t port;
	/** transactor state */
	uint16_t state;
	/** private data. */
	void *priv;
};

/** Convert an mtran state to a string
 *
 * @param state		The mtran state
 *
 * @return		A statically allocated string representing the state
 *			name
 */
extern const char *mtran_state_to_str(uint16_t state);

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
