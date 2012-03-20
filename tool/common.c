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

#include "core/process_ctx.h"
#include "msg/bsend.h"
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

struct tool_rrctx *tool_rrctx_alloc(void)
{
	struct tool_rrctx *rrc, *res;

	rrc = calloc(1, sizeof(struct tool_rrctx));
	if (!rrc)
		return ERR_PTR(ENOMEM);
	rrc->fb = fast_log_create(g_fast_log_mgr, "tool_rrctx");
	if (IS_ERR(rrc->fb)) {
		res = (struct tool_rrctx*)rrc->fb;
		free(rrc);
		return res;
	}
	rrc->ctx = bsend_init(rrc->fb, 1);
	if (IS_ERR(rrc->ctx)) {
		res = (struct tool_rrctx*)rrc->ctx;
		fast_log_free(rrc->fb);
		free(rrc);
		return res;
	}
	return rrc;

}

void tool_rrctx_free(struct tool_rrctx *rrc)
{
	fast_log_free(rrc->fb);
	bsend_free(rrc->ctx);
	free(rrc);
}
