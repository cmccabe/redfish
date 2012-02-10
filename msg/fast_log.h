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

#ifndef REDFISH_MSG_FAST_LOG_DOT_H
#define REDFISH_MSG_FAST_LOG_DOT_H

#include "util/fast_log.h"

#include <stdint.h> /* for uint32_t, etc. */

enum fast_log_msgr_event {
	FLME_MSGR_INIT = 1,
	FLME_MSGR_SHUTDOWN,
	FLME_MTRAN_SEND_NEXT,
	FLME_OOM,
	FLME_LISTENING,
	FLME_MAX_CONN_REACHED,
	FLME_DO_SOCKET_FAILED,
	FLME_INBOUND_CONN_CREATED,
	FLME_OUTBOUND_CONN_CREATED,
	FLME_READING_MSG_HEADER,
	FLME_CONN_ESTABLISHED,
	FLME_OUTGOING_CONN_FAILED,
	FLME_CONN_TIMED_OUT,
	FLME_CONN_REUSED,
	FLME_ACCEPT_FAILED,
	FLME_EV_ERROR,
	FLME_NO_EV_READ,
	FLME_MTRAN_MULTI_CONN,
	FLME_MTRAN_WRONG_REM_TRID,
	FLME_MTRAN_NONESUCH,
	FLME_EXPECTED_PENDING_TRANSACTOR,
	FLME_HDR_READ_ERROR,
	FLME_READ_ERROR,
	FLME_WRITE_ERROR,
	FLME_MAX,
};

extern void fast_log_msgr_impl(struct fast_log_buf *fb, uint16_t ty,
		uint16_t port, uint32_t ip,
		uint32_t trid, uint32_t rem_trid, uint16_t event,
		uint16_t event_data);

extern void fast_log_msgr_debug_dump(struct fast_log_entry *fe, char *buf);

extern void fast_log_msgr_info_dump(struct fast_log_entry *fe, char *buf);

extern void fast_log_msgr_warn_dump(struct fast_log_entry *fe, char *buf);

enum fast_log_bsend_event {
	FLBS_INIT = 1,
	FLBS_ADD_TR,
	FLBS_JOIN,
	FLBS_RESET,
	FLBS_FREE,
	FLBS_MAX,
};

extern void fast_log_bsend(struct fast_log_buf *fb,
		uint16_t ty, uint8_t event, uint16_t port, uint32_t ip,
		uint8_t flags, uint16_t err, uint32_t event_data);

extern void fast_log_bsend_debug_dump(struct fast_log_entry *fe, char *buf);

extern void fast_log_bsend_error_dump(struct fast_log_entry *fe, char *buf);

#endif
