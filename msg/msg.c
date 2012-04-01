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

#include "msg/msg.h"
#include "msg/types.h"
#include "msg/xdr.h"
#include "util/error.h"
#include "util/macro.h"
#include "util/net.h"
#include "util/packed.h"
#include "util/string.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

void mtran_ep_to_str(const struct mtran *tr, char *buf, size_t buf_len)
{
	char addr_str[INET_ADDRSTRLEN];

	ipv4_to_str(tr->ip, addr_str, sizeof(addr_str));
	snprintf(buf, buf_len, "[%s/%d]", addr_str, tr->port);
}

void *calloc_msg(uint32_t ty, uint32_t len)
{
	struct msg *m;

	m = calloc(1, len);
	if (!m)
		return NULL;
	pack_to_be32(&m->len, len);
	pack_to_be16(&m->ty, ty);
	pack_to_8(&m->refcnt, 1);
	return m;
}

struct msg *resp_alloc(int error)
{
	struct msg *r;
	struct mmm_resp resp;

	resp.error = FORCE_POSITIVE(error);
	r = MSG_XDR_ALLOC(mmm_resp, &resp);
	if (IS_ERR(r))
		return r;
	return r;
}

struct msg *msg_shrink(struct msg *m, uint32_t amt)
{
	uint32_t cur_len, new_len;
	struct msg *r;
	
	cur_len = unpack_from_be32(&m->len);
	if (cur_len < amt)
		abort();
	new_len = cur_len - amt;
	pack_to_be32(&m->len, new_len);
	r = realloc(m, new_len);
	return r ? r : m;
}

void msg_addref(struct msg *msg)
{
	int refcnt;

	refcnt = unpack_from_8(&msg->refcnt);
	++refcnt;
	if (refcnt > 0xff)
		abort();
	pack_to_8(&msg->refcnt, refcnt);
}

void msg_release(struct msg *msg)
{
	int refcnt;

	refcnt = unpack_from_8(&msg->refcnt);
	if (refcnt == 0)
		abort();
	--refcnt;
	if (refcnt == 0) {
		free(msg);
	}
	else {
		pack_to_8(&msg->refcnt, refcnt);
	}
}

int dump_msg_hdr(struct msg *msg, char *buf, size_t buf_len)
{
	return snprintf(buf, buf_len,
		"{trid=0x%08x, rem_trid=0x%08x, len=%d, ty=%d}",
		unpack_from_be32(&msg->trid),
		unpack_from_be32(&msg->rem_trid),
		unpack_from_be32(&msg->len),
		unpack_from_be16(&msg->ty));
}

void dump_msg(struct msg *m, char *buf, size_t buf_len)
{
	int ret;
	uint32_t len;

	len = unpack_from_be32(&m->len);
	if (len < sizeof(struct msg)) {
		snprintf(buf, buf_len, "(invalid msg");
		return;
	}
	len -= sizeof(struct msg);
	ret = dump_msg_hdr(m, buf, buf_len);
	if ((ret < 0) || (ret >= (int)buf_len))
		return;
	if (++ret >= (int)buf_len)
		return;
	buf[ret - 1] = ' ';
	hex_dump(m->data, len, buf + ret, buf_len - ret);
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

int msg_validate(const struct msg *m, uint16_t expected_ty, int min_size)
{
	uint16_t ty;
	uint32_t len;

	ty = unpack_from_be16(&m->ty);
	if (ty != expected_ty)
		return -EINVAL;
	len = unpack_from_be32(&m->len);
	if (len < (uint32_t)min_size)
		return -EINVAL;
	return 0;
}
