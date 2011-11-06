/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MSG_FAST_LOG_DOT_H
#define REDFISH_MSG_FAST_LOG_DOT_H

#include "util/fast_log.h"

#include <stdint.h> /* for uint32_t, etc. */

enum fast_log_msgr_event {
	FLME_MTRAN_SEND_NEXT = 1,
	FLME_OOM,
	FLME_LISTENING,
	FLME_MAX_CONN_REACHED,
	FLME_DO_SOCKET_FAILED,
	FLME_INBOUND_CONN_CREATED,
	FLME_OUTBOUND_CONN_CREATED,
	FLME_CONN_ESTABLISHED,
	FLME_OUTGOING_CONN_FAILED,
	FLME_CONN_REUSED,
	FLME_ACCEPT_FAILED,
	FLME_EV_ERROR,
	FLME_NO_EV_READ,
	FLME_MTRAN_MULTI_CONN,
	FLME_MTRAN_WRONG_REM_TRID,
	FLME_MTRAN_NONESUCH,
	FLME_MTRAN_NEW_STATE,
	FLME_EXPECTED_PENDING_TRANSACTOR,
	FLME_HDR_READ_ERROR,
	FLME_READ_ERROR,
};

extern void fast_log_msgr_impl(struct fast_log_buf *fb, uint16_t ty,
		uint16_t port, uint32_t ip,
		uint32_t trid, uint32_t rem_trid, uint16_t event,
		uint16_t event_data);

extern void fast_log_msgr_debug_dump(struct fast_log_entry *fe, char *buf);

extern void fast_log_msgr_info_dump(struct fast_log_entry *fe, char *buf);

extern void fast_log_msgr_warn_dump(struct fast_log_entry *fe, char *buf);

#endif
