/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "msg/msg.h"

#include <endian.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void *calloc_msg(uint32_t ty, uint32_t len)
{
	struct msg *m = calloc(1, len);
	if (!m)
		return NULL;
	len -= sizeof(struct msg);
	m->len = htobe32(len);
	m->ty = htobe16(ty);
	return m;
}

void dump_msg_hdr(struct msg *msg, char *buf, size_t buf_len)
{
	snprintf(buf, buf_len,
		"{trid=0x%08x, rem_trid=0x%08x, len=%d, ty=%d}",
		 be32toh(msg->trid), be32toh(msg->rem_trid),
		 be32toh(msg->len), be16toh(msg->ty));
}
