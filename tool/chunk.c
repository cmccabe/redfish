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

#include "client/fishc.h"
#include "common/cluster_map.h"
#include "common/config/unitaryc.h"
#include "core/glitch_log.h"
#include "msg/bsend.h"
#include "msg/msg.h"
#include "msg/types.h"
#include "msg/xdr.h"
#include "tool/common.h"
#include "tool/tool.h"
#include "util/error.h"
#include "util/packed.h"
#include "util/safe_io.h"
#include "util/str_to_int.h"
#include "util/terror.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TOOL_TIMEO 60

static struct chunk_op_ctx *chunk_op_ctx_alloc(struct fishtool_params *params);
static void chunk_op_ctx_free(struct chunk_op_ctx *cct);

struct chunk_op_ctx {
	struct tool_rrctx *rrc;
	uint64_t cid;
	int oid;
	int fd;
	struct osdc *osdc;
};

static struct chunk_op_ctx *chunk_op_ctx_alloc(struct fishtool_params *params)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct chunk_op_ctx *cct;
	const char *cid_str, *oid_str;
	int ret;

	cct = calloc(1, sizeof(struct chunk_op_ctx));
	if (!cct) {
		glitch_log("chunk_op_ctx_alloc: OOM\n");
		goto error;
	}
	cct->fd = -1;
	cid_str = params->non_option_args[0];
	if (!cid_str) {
		glitch_log("You must give a chunk ID to write "
			"to.  -h for help.");
		goto error_free_cct;
	}
	cct->cid = str_to_u64(cid_str, err, err_len);
	if (err[0]) {
		glitch_log("error parsing chunk ID: %s", err);
		goto error_free_cct;
	}
	oid_str = params->lowercase_args[ALPHA_IDX('k')];
	if (!oid_str) {
		glitch_log("You must specify what osd to write "
			"to.  -h for help.\n");
		goto error_free_cct;
	}
	cct->oid = str_to_int(oid_str, err, err_len);
	if (err[0]) {
		glitch_log("error parsing OSD ID: %s", err);
		goto error_free_cct;
	}
	cct->rrc = tool_rrctx_alloc(params->cpath);
	if (IS_ERR(cct->rrc)) {
		ret = PTR_ERR(cct->rrc);
		glitch_log("fishtool_chunk_write: error allocating "
			"rrctx: error %d (%s)\n", ret, terror(ret));
		goto error_free_cct;
	}
	cct->osdc = unitaryc_lookup_osdc(cct->rrc->conf, cct->oid);
	if (IS_ERR(cct->osdc)) {
		glitch_log("Error: no such OSD as %d found in the "
			"config.", cct->oid);
		goto error_free_rrc;
	}
	return cct;

error_free_rrc:
	tool_rrctx_free(cct->rrc);
error_free_cct:
	chunk_op_ctx_free(cct);
error:
	return NULL;
}

static void chunk_op_ctx_free(struct chunk_op_ctx *cct)
{
	int POSSIBLY_UNUSED(res);

	if (!IS_ERR(cct->rrc))
		tool_rrctx_free(cct->rrc);
	if ((cct->fd != STDIN_FILENO) && (cct->fd >= 0)) {
		RETRY_ON_EINTR(res, close(cct->fd));
	}
	free(cct);
}

static int do_chunk_write(struct chunk_op_ctx *cct, const char *buf,
		size_t buf_len)
{
	int32_t ret;
	struct mmm_osd_hflush_req req;
	struct msg *m;
	struct daemon_info *oinfo;
	struct mtran *tr;
	char *extra;

	oinfo =  cmap_get_oinfo(cct->rrc->cmap, cct->oid);
	if (!oinfo)
		return -EINVAL;
	req.cid = cct->cid;
	req.flags = 0;
	m = msg_xdr_extalloc(mmm_osd_hflush_req_ty,
		(xdrproc_t)xdr_mmm_osd_hflush_req,
		&req, buf_len, (void**)&extra);
	if (IS_ERR(m))
		return FORCE_NEGATIVE(PTR_ERR(m));
	memcpy(extra, buf, buf_len);
	bsend_add(cct->rrc->ctx, cct->rrc->msgr, BSF_RESP, m,
		oinfo->ip, oinfo->port[RF_ENTITY_TY_CLI], TOOL_TIMEO);
	bsend_join(cct->rrc->ctx);
	tr = bsend_get_mtran(cct->rrc->ctx, 0);
	if (IS_ERR(tr->m)) {
		bsend_reset(cct->rrc->ctx);
		glitch_log("error sending MMM_OSD_HFLUSH_REQ: error %d\n",
			PTR_ERR(tr->m));
		return FORCE_NEGATIVE(PTR_ERR(tr->m));
	}
	ret = msg_xdr_decode_as_generic(tr->m);
	if (ret < 0) {
		glitch_log("invalid reply from server-- can't understand "
			"response type %d\n", unpack_from_be16(&tr->m->ty));
		ret = -EIO;
		bsend_reset(cct->rrc->ctx);
		return FORCE_NEGATIVE(ret);
	}
	bsend_reset(cct->rrc->ctx);
	return ret;
}

int fishtool_chunk_write(struct fishtool_params *params)
{
	int ret;
	struct chunk_op_ctx *cct = NULL;
	const char *local;

	cct = chunk_op_ctx_alloc(params);
	if (!cct) {
		ret = -EIO;
		goto done;
	}
	local = params->lowercase_args[ALPHA_IDX('i')];
	if (local) {
		cct->fd = open(local, O_RDONLY);
		if (cct->fd < 0) {
			ret = -errno;
			fprintf(stderr, "error opening "
				"'%s' for read: %d\n", local, ret);
			goto done;
		}
	}
	else {
		local = "stdin";
		cct->fd = STDIN_FILENO;
	}
	while (1) {
		int amt;
		char buf[8192];

		amt = safe_read(cct->fd, buf, sizeof(buf));
		if (amt < 0) {
			ret = amt;
			fprintf(stderr, "error reading "
				"%s: %d\n", local, ret);
			goto done;
		}
		if (amt == 0) {
			break;
		}
		ret = do_chunk_write(cct, buf, amt);
		if (ret) {
			fprintf(stderr, "do_chunk_write: error writing "
				"%d bytes to %s: %d\n", amt, local, ret);
			goto done;
		}
	}
	ret =0;
done:
	if (!IS_ERR(cct))
		chunk_op_ctx_free(cct);
	return ret;
}

static const char *fishtool_chunk_write_usage[] = {
	"chunk write: write directly to a chunk on an OSD.",
	"",
	"usage:",
	"chunk_write [options] <chunk-ID>",
	"",
	"options:",
	"-i <file>      input file",
	"               If no local file is given, stdin will be used.",
	"-k <oid>       OSD ID to contact",
	NULL,
};

struct fishtool_act g_fishtool_chunk_write = {
	.name = "chunk_write",
	.fn = fishtool_chunk_write,
	.getopt_str = "i:k:",
	.usage = fishtool_chunk_write_usage,
};

static int do_chunk_read(struct chunk_op_ctx *cct, const char *fname,
		int fd, uint64_t start, uint32_t len)
{
	int32_t ret, m_len;
	struct mmm_osd_read_req req;
	struct msg *m;
	struct mmm_osd_read_resp rr;
	struct daemon_info *oinfo;
	struct mtran *tr;
	const char *extra;

	oinfo =  cmap_get_oinfo(cct->rrc->cmap, cct->oid);
	if (!oinfo)
		return -EINVAL;
	req.cid = cct->cid;
	req.start = start;
	req.len = len;
	m = MSG_XDR_ALLOC(mmm_osd_read_req, &req);
	if (IS_ERR(m))
		return PTR_ERR(m);
	bsend_add(cct->rrc->ctx, cct->rrc->msgr, BSF_RESP, m,
		oinfo->ip, oinfo->port[RF_ENTITY_TY_CLI], TOOL_TIMEO);
	bsend_join(cct->rrc->ctx);
	tr = bsend_get_mtran(cct->rrc->ctx, 0);
	if (IS_ERR(tr->m)) {
		ret = FORCE_NEGATIVE(PTR_ERR(tr->m));
		glitch_log("error sending MMM_OSD_READ_REQ: error %d\n", ret);
		goto done;
	}
	ret = msg_xdr_decode_as_generic(tr->m);
	if (ret > 0) {
		glitch_log("error reply from server: error %d\n", ret);
		goto done;
	}
	m_len = msg_xdr_extdecode((xdrproc_t)xdr_mmm_osd_read_resp, tr->m,
		&rr, (const void**)&extra);
	if (m_len < 0) {
		ret = -EIO;
		glitch_log("invalid reply from server-- can't understand "
			"response type %d\n", unpack_from_be16(&tr->m->ty));
		goto done;
	}
	ret = FORCE_NEGATIVE(safe_write(fd, extra, m_len));
	xdr_free((xdrproc_t)xdr_mmm_osd_read_resp, (void*)&rr);
	if (ret) {
		glitch_log("error writing to %s: error %d (%s)\n",
			fname, ret, terror(ret));
		goto done;
	}
	ret = m_len;
done:
	bsend_reset(cct->rrc->ctx);
	return ret;
}

int fishtool_chunk_read(struct fishtool_params *params)
{
	int ret;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct chunk_op_ctx *cct = NULL;
	const char *local;
	const char *len_str;
	const char *start_str;
	uint64_t start, len;

	cct = chunk_op_ctx_alloc(params);
	if (!cct) {
		ret = -EIO;
		goto done;
	}
	start_str = params->lowercase_args[ALPHA_IDX('s')];
	if (start_str) {
		start = str_to_u64(start_str, err, err_len);
		if (err[0]) {
			glitch_log("error parsing start: %s", err);
			ret = -EINVAL;
			goto done;
		}
	}
	else {
		start = 0;
	}
	len_str = params->lowercase_args[ALPHA_IDX('l')];
	if (len_str) {
		len = str_to_u64(len_str, err, err_len);
		if (err[0]) {
			glitch_log("error parsing len: %s", err);
			ret = -EINVAL;
			goto done;
		}
	}
	else {
		/* If no length is given, read as much as we can. */
		len = 0xffffffffffffffffLLU;
	}
	local = params->lowercase_args[ALPHA_IDX('o')];
	if (local) {
		cct->fd = open(local, O_WRONLY | O_TRUNC | O_CREAT, 0644);
		if (cct->fd < 0) {
			ret = -errno;
			fprintf(stderr, "error opening "
				"'%s' for write: %d\n", local, ret);
			goto done;
		}
	}
	else {
		local = "stdout";
		cct->fd = STDOUT_FILENO;
	}
	while (1) {
		int amt;
		char buf[8192];

		amt = (len > sizeof(buf)) ? sizeof(buf) : len;
		ret = do_chunk_read(cct, local, cct->fd, start, amt);
		if (ret < 0) {
			glitch_log("do_chunk_read: error reading "
				"%d bytes to from offset 0x%"PRIx64": %d\n",
				amt, start, ret);
			goto done;
		}
		if (ret != sizeof(buf))
			break;
		start += sizeof(buf);
		len -= sizeof(buf);
	}
	ret = 0;
done:
	if (!IS_ERR(cct))
		chunk_op_ctx_free(cct);
	return ret;
}

static const char *fishtool_chunk_read_usage[] = {
	"chunk read: read directly from a chunk on an OSD.",
	"",
	"usage:",
	"chunk_read [options] <chunk-ID>",
	"",
	"options:",
	"-k <oid>       OSD ID to contact",
	"-l <length>    maximum number of bytes to read (default: all)",
	"-o <file>      output file",
	"               If no local file is given, stdout will be used.",
	"-s <start>     starting offset within the chunk (default: 0)",
	NULL,
};

struct fishtool_act g_fishtool_chunk_read = {
	.name = "chunk_read",
	.fn = fishtool_chunk_read,
	.getopt_str = "k:l:o:s:",
	.usage = fishtool_chunk_read_usage,
};
