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

#ifndef REDFISH_TOOL_COMMON_DOT_H
#define REDFISH_TOOL_COMMON_DOT_H

struct tool_rrctx {
	/** Fast log buffer */
	struct fast_log_buf *fb;
	/** Blocking RPC context */
	struct bsend *ctx;
};

/** Allocate a raw rpc context
 *
 * @return		The new raw rpc context, or an error pointer
 */
extern struct tool_rrctx *tool_rrctx_alloc(void);

/** Free a raw rpc context
 *
 * @param rc		The raw rpc context
 */
extern void tool_rrctx_free(struct tool_rrctx *rrc);

#endif
