/*
 * vim: ts=8:sw=8:tw=79:noet
 * 
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

#include "common/cluster_map.h"
#include "common/config/unitaryc.h"
#include "core/glitch_log.h"
#include "core/process_ctx.h"
#include "mds/const.h"
#include "msg/bsend.h"
#include "msg/msgr.h"
#include "tool/common.h"
#include "util/error.h"
#include "util/fast_log.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct tool_rrctx *tool_rrctx_alloc(const char *cpath)
{
	int ret;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	const struct msgr_conf mconf = {
		.max_conn = 1024,
		.max_tran = 1024,
		.tcp_teardown_timeo = 10,
		.name = "tool_rrctx_msgr",
		.fl_mgr = g_fast_log_mgr,
	};
	struct tool_rrctx *rrc;

	rrc = calloc(RF_MAX_MDS, sizeof(struct tool_rrctx));
	if (!rrc) {
		ret = ENOMEM;
		goto err;
	}
	rrc->fb = fast_log_create(g_fast_log_mgr, "tool_rrctx");
	if (IS_ERR(rrc->fb)) {
		glitch_log("Error parsing config file: %s\n", err);
		ret = PTR_ERR(rrc->fb);
		goto err_free_rrc;
	}
	rrc->conf = parse_unitary_conf_file(cpath, err, err_len);
	if (err[0]) {
		glitch_log("Error parsing config file: %s\n", err);
		ret = -EINVAL;
		goto err_free_fb;
	}
	harmonize_unitary_conf(rrc->conf, err, err_len);
	if (err[0]) {
		glitch_log("Error harmonizing config file: %s", err);
		ret = -EINVAL;
		goto err_free_conf;
	}
	rrc->cmap = cmap_from_conf(rrc->conf, err, err_len);
	if (err[0]) {
		glitch_log("Error creating cluster map: %s", err);
		ret = -EIO;
		goto err_free_conf;
	}
	rrc->ctx = bsend_init(rrc->fb, 1);
	if (IS_ERR(rrc->ctx)) {
		ret = PTR_ERR(rrc->ctx);
		goto err_free_cmap;
	}
	rrc->msgr = msgr_init(err, err_len, &mconf);
	if (IS_ERR(rrc->msgr)) {
		ret = PTR_ERR(rrc->msgr);
		goto err_free_ctx;
	}
	msgr_start(rrc->msgr, err, err_len);
	if (err[0]) {
		glitch_log("Error starting messenger: %s\n", err);
		ret = -EIO;
		goto err_free_msgr;
	}
	return rrc;

err_free_msgr:
	msgr_free(rrc->msgr);
err_free_ctx:
	bsend_free(rrc->ctx);
err_free_cmap:
	cmap_free(rrc->cmap);
err_free_conf:
	free_unitary_conf_file(rrc->conf);
err_free_fb:
	fast_log_free(rrc->fb);
err_free_rrc:
	free(rrc);
err:
	return ERR_PTR(FORCE_POSITIVE(ret));
}

void tool_rrctx_free(struct tool_rrctx *rrc)
{
	msgr_shutdown(rrc->msgr);
	msgr_free(rrc->msgr);
	bsend_free(rrc->ctx);
	free_unitary_conf_file(rrc->conf);
	cmap_free(rrc->cmap);
	fast_log_free(rrc->fb);
	free(rrc);
}
