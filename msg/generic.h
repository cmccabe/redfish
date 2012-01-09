/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MSG_GENERIC_DOT_H
#define REDFISH_MSG_GENERIC_DOT_H

#include "msg/msg.h"
#include "util/compiler.h"

#include <stdint.h>
#include <unistd.h> /* for size_t */

/* Network messages that can be sent to anyone */

enum {
	/** Acknowledge request */
	MMM_ACK = 1000,
	/** Deny request */
	MMM_NACK,
};

PACKED(
struct mmm_nack {
	struct msg base;
	uint32_t error;
});

/* stat information for a file or directory
 *
 * 'A packed stat array' is just a series of mmm_stat structures put together
 * without any padding. At the very end is 16 bits of 0.  (This exploits the
 * fact that stat_len cannot be 0 for a valid stat entry.)
 */
PACKED(
struct mmm_stat_hdr {
	uint16_t stat_len;
	uint16_t mode_and_type;
	uint32_t uid;
	uint32_t gid;
	uint32_t block_sz;
	uint64_t mtime;
	uint64_t atime;
	uint64_t length;
	uint8_t man_repl;
	char data[0];
	/* pcomp */
});

extern int send_nack(int fd, int error);

extern int send_ack(int fd);

#endif
