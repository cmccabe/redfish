/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "msg/generic.h"
#include "msg/msg.h"
#include "util/packed.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void *calloc_msg(uint32_t ty, uint32_t len)
{
	struct msg *m;

	m = calloc(1, len);
	if (!m)
		return NULL;
	len -= sizeof(struct msg);
	pack_to_be32(&m->len, len);
	pack_to_be16(&m->ty, ty);
	return m;
}

struct msg *calloc_nack_msg(uint32_t error)
{
	struct msg *m;
	struct mmm_nack *mout;

	m = calloc_msg(MMM_NACK, sizeof(struct mmm_nack));
	if (!m)
		return NULL;
	mout =(struct mmm_nack*)m;
	pack_to_be32(&mout->error, error);
	return m;
}

void dump_msg_hdr(struct msg *msg, char *buf, size_t buf_len)
{
	snprintf(buf, buf_len,
		"{trid=0x%08x, rem_trid=0x%08x, len=%d, ty=%d}",
		unpack_from_be32(&msg->trid),
		unpack_from_be32(&msg->rem_trid),
		unpack_from_be32(&msg->len),
		unpack_from_be16(&msg->ty));
}
