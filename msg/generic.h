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
 * 'A packed stat array' is just a series of mmm_stat structures put
 * together without any padding. At the very end is 16 bits of 0.
 * (This exploits the fact that path_len cannot be 0 for a valid mmm_stat
 * entry.)
 */
PACKED(
struct mmm_stat {
	struct msg base;
	uint16_t path_len;
	uint16_t mode;
	uint32_t block_sz;
	int64_t mtime;
	int64_t atime;
	int64_t length;
	uint8_t is_dir;
	uint8_t repl;
	uint16_t owner;
	uint16_t group;
	char path[0];
});
PACKED(
struct mmm_path {
	struct msg base;
	uint16_t len;
	char str[0];
});

extern int safe_read_path(char *path_out, size_t path_max,
			struct mmm_path *path, int fd);

extern int send_nack(int fd, int error);

extern int send_ack(int fd);

#endif
