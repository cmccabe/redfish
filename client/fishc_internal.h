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

#ifndef REDFISH_CLIENT_FISHC_IMPL_DOT_H
#define REDFISH_CLIENT_FISHC_IMPL_DOT_H

#include "client/fishc.h" /* for redfish_log_fn_t */

#include "util/compiler.h"

/** Redfish replication count
 * TODO: make this adjustable
 */
#define REDFISH_FIXED_REPL 3

/** Redfish fixed 64 MB local buffer size
 * TODO: make this adjustable
 */
#define REDFISH_FIXED_LBUF_SZ 67108864

/** Redfish fixed 64 MB chunk size
 * TODO: make this adjustable
 */
#define REDFISH_FIXED_BLOCK_SZ 67108864

/** Default mode for files */
#define REDFISH_DEFAULT_FILE_MODE 0644

/** Default mode for files */
#define REDFISH_DEFAULT_DIR_MODE 0755

/** Log to a client's callback function
 *
 * @param log_cb	The log callback to use
 * @param log_ctx	The log callback context to use
 * @param fmt		Printf-style format string
 * @param ...		Printf-style arguments
 */
extern void client_log(redfish_log_fn_t log_cb, void *log_ctx,
		const char *fmt, ...) PRINTF_FORMAT(3, 4);

#define CLIENT_LOG(cli, ...) client_log(cli->log_cb, cli->log_ctx, __VA_ARGS__)

#endif
