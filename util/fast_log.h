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

#ifndef REDFISH_UTIL_FAST_LOG_DOT_H
#define REDFISH_UTIL_FAST_LOG_DOT_H

#include "util/compiler.h"

#include <stdint.h>

/* fast_log is a fast circular buffer used to log routine informational
 * messages during normal daemon operation. The intention is that each thread
 * will have its own fast_log_buf, which it can log its events to.
 *
 * When a crash occurs, we can dump the fast_logs to a file in order to get some
 * insight into what each thread was doing at the moment of the crash.
 * This gives us most of the advantages of using extensive logging, without the
 * huge performance problems of actually writing all those logs to a file
 * constantly. fast_log is active in production mode and is never turned off.
 *
 * The monitor can also request a fast_log dump.
 *
 * fast_log messages are stored in a compact serialized form. Each message must
 * be exactly 32 bytes. The first 2 bytes are a type code. You must register a
 * dumper function to decode the message into a human-readable form during
 * dumping.
 */
struct fast_log_buf;
struct fast_log_entry;
struct fast_log_mgr;

/** A function used to pretty-print a fast_log entry to a file descriptor.
 *
 * Note: this must be suitable to use in a signal handler.
 * Use only signal-safe functions. memset and memcpy are also allowed here.
 *
 * @param fe		the fast_log entry
 * @param buf		output buffer of size FAST_LOG_PRETTY_PRINTED_MAX
 */
typedef void (*fast_log_dumper_fn_t)(struct fast_log_entry *fe, char *buf);

/** A function used to store a pretty-printed fast log in a more permanent
 * fashion.
 *
 * @param store_ctx	The context pointer that was supplied to fast_log_mgr
 * @param str		The pretty-printed fast_log entry. Will never be longer
 *			than FAST_LOG_PRETTY_PRINTED_MAX
 */
typedef void (*fast_log_storage_fn_t)(void *store_ctx, const char *str);

/** Maximum pretty-printed size of a fast log entry */
#define FAST_LOG_PRETTY_PRINTED_MAX 512

/** Maximum length of a fast_log buffer name. */
#define FAST_LOG_BUF_NAME_MAX 24

/** Maximum number of fast_log types */
#define FAST_LOG_TYPE_MAX 64

/** Represents a fast_log entry.
 * All fast_log entries have the same size.
 * The first 4 bytes always identify the type.
 */
PACKED_ALIGNED(8,
struct fast_log_entry
{
        /** The fast_log message type */
	uint16_t type;

        /** The fast_log message data */
        uint8_t rem[30];
}
);

/** Create a fast_log buffer.
 *
 * @param mgr		The fast log manager
 * @param fbname	The name of the fast_log buffer to create. If it is
 *			longer than fast_log_BUF_NAME_MAX, it will be truncated
 *
 * @return		The fast_log on success, or an error pointer on failure
 */
extern struct fast_log_buf* fast_log_create(struct fast_log_mgr *mgr,
					const char *fbname);

/** Set the name of the fast log buffer
 *
 * @param fb		The fast log buffer
 * @param name		The new name.  This will be deep-copied.
 */
extern void fast_log_set_name(struct fast_log_buf *fb, const char *name);

/** Destroys a fast_log buffer.
 *
 * @param fb		The fast_log buffer
 */
extern void fast_log_free(struct fast_log_buf* fb);

/** Output a fast_log message.
 *
 * @param fb		The fast_log buffer to use
 * @param fe		The fast_log message to output. Must have
 *			sizeof(struct fast_log_entry)
 */
extern void fast_log(struct fast_log_buf* fb, void *fe);

/** Copy one fast log buffer to another
 *
 * This function is signal-safe.
 *
 * @param dst		destination fast log buffer
 * @param src		source fast log buffer
 */
void fast_log_copy(struct fast_log_buf *dst,
		const struct fast_log_buf *src);

/** Dump the fast_log
 *
 * @param fb		the fast log buffer to dump
 * @param dumpers	the dumper functions to use
 * @param fb		file descriptor to dump to
 *
 * @return		0 on success; error code otherwise
 */
extern int fast_log_dump(const struct fast_log_buf* fb,
		const fast_log_dumper_fn_t *dumpers, int fd);

#endif
