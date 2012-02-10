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

#include "msg/fast_log.h"
#include "msg/generic.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/macro.h"
#include "util/net.h"
#include "util/string.h"

#include <arpa/inet.h>
#include <errno.h>
#include <ev.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PACKED(
struct fast_log_msgr_entry {
	uint16_t ty;
	uint16_t port;
	uint32_t ip;
	uint32_t trid;
	uint32_t rem_trid;
	uint16_t event;
	uint16_t event_data;
	char pad[12];
});

BUILD_BUG_ON(FLME_MAX > 0xffff);

void fast_log_msgr_impl(struct fast_log_buf *fb,
		uint16_t ty, uint16_t port, uint32_t ip,
		uint32_t trid, uint32_t rem_trid, uint16_t event,
		uint16_t event_data)
{
	struct fast_log_msgr_entry fe;
	memset(&fe, 0, sizeof(fe));
	fe.ty = ty;
	fe.port = port;
	fe.ip = ip;
	fe.trid = trid;
	fe.rem_trid = rem_trid;
	fe.event = event;
	fe.event_data = event_data;
	fast_log(fb, &fe);
}

const char *event_data_to_immediately_or_normally(uint16_t event_data)
{
	return event_data ? "immediately" : "normally";
}

void fast_log_msgr_dump(struct fast_log_msgr_entry *fe, char *buf)
{
	char addr_str[INET_ADDRSTRLEN];

	ipv4_to_str(fe->ip, addr_str, sizeof(addr_str));
	snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
		"[%s/%d] (%08x:%08x) ", addr_str, fe->port,
		fe->trid, fe->rem_trid);
	switch (fe->event) {
	case FLME_MSGR_INIT:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			 "Created new messenger thread %d.\n", fe->event_data);
		break;
	case FLME_MSGR_SHUTDOWN:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			 "Shutting down messenger thread %d.\n", fe->event_data);
		break;
	case FLME_MTRAN_SEND_NEXT:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			 "mtran_send of message type %d\n", fe->event_data);
		break;
	case FLME_OOM:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			 "out of memory at point %d\n", fe->event_data);
		break;
	case FLME_LISTENING:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			 "messenger listening on port %d\n", fe->event_data);
		break;
	case FLME_MAX_CONN_REACHED:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"failed to create another connection: already "
			"have %d connections\n", fe->event_data);
		break;
	case FLME_DO_SOCKET_FAILED:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"failed to create socket: got error %d\n",
			fe->event_data);
		break;
	case FLME_INBOUND_CONN_CREATED:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"created new inbound connection\n");
		break;
	case FLME_OUTBOUND_CONN_CREATED:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"created new outbound connection for msg of type %d\n",
			fe->event_data);
		break;
	case FLME_READING_MSG_HEADER:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"reading message header (recv_cnt = %d)\n",
			fe->event_data);
		break;
	case FLME_CONN_ESTABLISHED:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"connect(2) operation succeeded %s\n",
			event_data_to_immediately_or_normally(fe->event_data));
		break;
	case FLME_OUTGOING_CONN_FAILED:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"outgoing connect(2) failed: error %d\n",
			fe->event_data);
		break;
	case FLME_CONN_TIMED_OUT:
		if (fe->event_data == 0) {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"TCP connection closed after timeout.\n");
		}
		else {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"TCP connection timeout delievered to %d "
				"transactors\n", fe->event_data);
		}
		break;
	case FLME_CONN_REUSED:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"assigning transactor to outbound connection for "
			"msg of type %d\n",fe->event_data);
		break;
	case FLME_ACCEPT_FAILED:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"accept failed with error %d\n", fe->event_data);
		break;
	case FLME_EV_ERROR:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"got EV_ERROR\n");
		break;
	case FLME_NO_EV_READ:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"didn't get EV_READ\n");
		break;
	case FLME_MTRAN_MULTI_CONN:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"We already have an existing connection from this "
			"remote. We don't want another one.\n");
		break;
	case FLME_MTRAN_WRONG_REM_TRID:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"Mismatched remote transactor ID.\n");
		break;
	case FLME_MTRAN_NONESUCH:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"No such transactor ID.\n");
		break;
	case FLME_EXPECTED_PENDING_TRANSACTOR:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"mconn_writable_cb: internal error: the writable "
			"callback was activated, but there was nothing "
			"pending.\n");
		break;
	case FLME_HDR_READ_ERROR:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"error reading message header: error %d\n",
			fe->event_data);
		break;
	case FLME_READ_ERROR:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"error reading message body: error %d\n",
			fe->event_data);
		break;
	case FLME_WRITE_ERROR:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			"error writing message body: error %d\n",
			fe->event_data);
		break;
	default:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			 "(unknown event %d\n)", fe->event);
		break;
	}
}

void fast_log_msgr_debug_dump(struct fast_log_entry *fe, char *buf)
{
	struct fast_log_msgr_entry *entry = (struct fast_log_msgr_entry*)fe;
	fast_log_msgr_dump(entry, buf);
}

void fast_log_msgr_info_dump(struct fast_log_entry *fe, char *buf)
{
	struct fast_log_msgr_entry *entry = (struct fast_log_msgr_entry*)fe;
	fast_log_msgr_dump(entry, buf);
}

void fast_log_msgr_error_dump(struct fast_log_entry *fe, char *buf)
{
	struct fast_log_msgr_entry *entry = (struct fast_log_msgr_entry*)fe;
	fast_log_msgr_dump(entry, buf);
}

BUILD_BUG_ON(FLBS_MAX > 0xff);

PACKED(
struct fast_log_bsend_entry {
	uint16_t ty;
	uint16_t port;
	uint32_t ip;
	uint8_t event;
	uint8_t flags;
	uint16_t err;
	uint32_t event_data;
	char pad[16];
});

void fast_log_bsend(struct fast_log_buf *fb,
		uint16_t ty, uint8_t event, uint16_t port, uint32_t ip,
		uint8_t flags, uint16_t err, uint32_t event_data)
{
	struct fast_log_bsend_entry fe;
	memset(&fe, 0, sizeof(fe));
	fe.ty = ty;
	fe.port = port;
	fe.ip = ip;
	fe.event = event;
	fe.flags = flags;
	fe.err = err;
	fe.event_data = event_data;
	fast_log(fb, &fe);
}

void fast_log_bsend_dump(struct fast_log_bsend_entry *fe, char *buf)
{
	char addr_str[INET_ADDRSTRLEN];

	ipv4_to_str(fe->ip, addr_str, sizeof(addr_str));
	snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
		"[%s/%d] ", addr_str, fe->port);
	switch (fe->event) {
	case FLBS_INIT:
		if (fe->err) {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"bsend_init: failed to create "
				"blocking send context: error %d\n",
				fe->err);
		}
		else {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"bsend_init: created blocking send context "
				"with max_tr = %d\n", fe->event_data);
		}
		break;
	case FLBS_ADD_TR:
		if (fe->err == EMFILE) {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"bsend_add_tr_or_free: failed to add "
				"transactor %d: reached max transactors for "
				"this bsend context.\n", fe->event_data);
		}
		else if (fe->err == ECANCELED) {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"bsend_add_tr_or_free: failed to add "
				"transactor %d: bsend context was cancelled.\n",
				fe->event_data);
		}
		else if (fe->err) {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"bsend_add_tr_or_free: failed to add "
				"transactor %d: error %d\n",
				fe->event_data, fe->err);
		}
		else {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"bsend_add_tr_or_free: added "
				"transactor %d\n", fe->event_data);
		}
		break;
	case FLBS_JOIN:
		if (fe->err == ECANCELED) {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				 "bsend_join: bsend context was cancelled.\n");
		}
		else if (fe->err) {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				 "bsend_join: unknown error %d\n", fe->err);
		}
		else if (fe->flags == 1) {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"bsend_join: finished all %d transactors\n",
				fe->event_data);
		}
		else {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"bsend_join: waiting for %d transactors\n",
				fe->event_data);
		}
		break;
	case FLBS_RESET:
		if (fe->err == EINVAL) {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
				"bsend_reset: LOGIC ERROR!  Resetting a bsend "
				"context that still has %d unfinished "
				"transactors.\n", fe->event_data);
		}
		else {
			snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX, "bsend_reset\n");
		}
		break;
	case FLBS_FREE:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX, "bsend_free\n");
		break;
	default:
		snappend(buf, FAST_LOG_PRETTY_PRINTED_MAX,
			 "(unknown event %d)\n", fe->event);
		break;
	}
}

void fast_log_bsend_debug_dump(struct fast_log_entry *fe, char *buf)
{
	struct fast_log_bsend_entry *entry = (struct fast_log_bsend_entry*)fe;
	fast_log_bsend_dump(entry, buf);
}

void fast_log_bsend_error_dump(struct fast_log_entry *fe, char *buf)
{
	struct fast_log_bsend_entry *entry = (struct fast_log_bsend_entry*)fe;
	fast_log_bsend_dump(entry, buf);
}
