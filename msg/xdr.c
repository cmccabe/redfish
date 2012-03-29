/*
 * Copyright 2012 the Redfish authors
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

#include "msg/fish_internal.h"
#include "msg/msg.h"
#include "msg/xdr.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/packed.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <rpc/xdr.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct msg* msg_xdr_alloc(uint16_t ty, xdrproc_t xdrproc, void *payload)
{
	void POSSIBLY_UNUSED(*v);
	return msg_xdr_extalloc(ty, xdrproc, payload, 0, &v);
}

struct msg* msg_xdr_extalloc(uint16_t ty, xdrproc_t xdrproc, void *payload,
		size_t extra_len, void **extra)
{
	struct msg *m;
	size_t xl;
	uint32_t len;
	XDR xdrs;

	xl = xdr_sizeof(xdrproc, payload);
	len = sizeof(struct msg) + xl + extra_len;
	m = calloc(1, len);
	if (!m)
		return ERR_PTR(ENOMEM);
	pack_to_be32(&m->len, len);
	pack_to_be16(&m->ty, ty);
	pack_to_8(&m->refcnt, 1);
	xdrmem_create(&xdrs, (void*)&m->data, xl, XDR_ENCODE);
	if (!xdrproc(&xdrs, (void*)payload)) {
		free(m);
		xdr_destroy(&xdrs);
		return ERR_PTR(EINVAL);
	}
	xdr_destroy(&xdrs);
	*extra = m->data + xl;
	return m;
}

int32_t msg_xdr_extdecode(xdrproc_t xdrproc, const struct msg *m,
		void *out, const void **extra)
{
	size_t cl;
	uint32_t xl;
	XDR xdrs;

	xl = unpack_from_be32(&m->len) - sizeof(struct msg);
	if (xl > 0x7fffffff)
		return -EINVAL;
	xdrmem_create(&xdrs, (void*)&m->data, xl, XDR_DECODE);
	if (!xdrproc(&xdrs, out)) {
		xdr_destroy(&xdrs);
		return -EINVAL;
	}
	cl = xdr_getpos(&xdrs);
	xdr_destroy(&xdrs);
	*extra = ((char*)out) + cl;
	return xl - cl;
}

int32_t msg_xdr_decode(xdrproc_t xdrproc, const struct msg *m,
		void *out)
{
	const void *POSSIBLY_UNUSED(extra);
	return msg_xdr_extdecode(xdrproc, m, out, &extra);
}

int32_t msg_xdr_decode_as_generic(const struct msg *m)
{
	int32_t ret;
	struct mmm_resp resp;

	ret = MSG_XDR_DECODE(mmm_resp, m, &resp);
	if (ret < 0)
		return ret;
	return resp.error;
}
