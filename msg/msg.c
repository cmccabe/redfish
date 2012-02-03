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

#include "msg/generic.h"
#include "msg/msg.h"
#include "util/packed.h"
#include "util/macro.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

void *calloc_msg(uint32_t ty, uint32_t len)
{
	struct msg *m;

	m = calloc(1, len);
	if (!m)
		return NULL;
	pack_to_be32(&m->len, len);
	pack_to_be16(&m->ty, ty);
	return m;
}

struct msg *copy_msg(const struct msg *m)
{
	struct msg *m2;
	uint32_t len;

	len = unpack_from_be32(&m->len);
	m2 = malloc(len);
	memcpy(m2, m, len);
	return m2;
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

const char *mtran_state_to_str(uint16_t state)
{
	switch (state) {
	case MTRAN_STATE_IDLE:
		return "MTRAN_STATE_IDLE";
	case MTRAN_STATE_SENDING:
		return "MTRAN_STATE_SENDING";
	case MTRAN_STATE_SENT:
		return "MTRAN_STATE_SENT";
	case MTRAN_STATE_ACTIVE:
		return "MTRAN_STATE_ACTIVE";
	case MTRAN_STATE_RECV:
		return "MTRAN_STATE_RECV";
	default:
		break;
	}
	return "(unknown)";
}

BUILD_BUG_ON(sizeof(in_addr_t) != sizeof(uint32_t));

int get_localhost_ipv4(uint32_t *lh)
{
	in_addr_t res;

	res = inet_addr("127.0.0.1");
	if (res == INADDR_NONE)
		return -ENOENT;
	*lh = ntohl(res);
	return 0;
}
