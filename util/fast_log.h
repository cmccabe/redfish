/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_CORE_fast_log_H
#define REDFISH_CORE_fast_log_H

#include "util/compiler.h"

#include <stdint.h>

/** fast_log is a fast circular buffer used to log routine informational
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

/** A function used to pretty-print a fast_log entry to a file descriptor.
 *
 * Note: this must be suitable to use in a signal handler.
 * Use only signal-safe functions. memset and memcpy are also allowed here.
 *
 * @param fe		the fast_log entry
 * @param fd		the output file descriptor
 *
 * Returns 0 on success; error code otherwise.
 */
typedef int (*fast_log_dumper_fn_t)(struct fast_log_entry *fe, int fd);

/** Finish initializing the fast_log infrastructure.
 *
 * This must be called before any calls to fast_log_create or other fast_log
 * functions.
 *
 * @param dumpers	Array mapping fast_log message IDs to dumper functions.
 * 			We will keep a pointer to this parameter.
 *
 * @return		0 on success; error code otherwise.
 */
extern int fast_log_init(const fast_log_dumper_fn_t *dumpers);

/** Initializes a fast_log buffer.
 *
 * @param fbname	The name of the fast_log buffer to create. If it is
 *			longer than fast_log_BUF_NAME_MAX, it will be truncated.
 *
 * @return		The fast_log on success, or NULL on failure.
 */
extern struct fast_log_buf* fast_log_create(const char *fbname);

/** Registers a fast log buffer to be dumped by fast_log_dump_all
 *
 * @param fb		The fastlog buffer to register
 *
 * @return		0 on success; error code otherwise
 */
extern int fast_log_register_buffer(struct fast_log_buf *fb);

/** Destroys a fast_log buffer.
 *
 * @param fb		The fast_log buffer
 */
extern void fast_log_destroy(struct fast_log_buf* fb);

/** Output a fast_log message.
 *
 * @param fb		The fast_log buffer to use
 * @param fe		The fast_log message to output. Must have
 *			sizeof(struct fast_log_entry)
 */
extern void fast_log(struct fast_log_buf* fb, void *fe);

/** Dump the fast_log
 *
 * @param fb		the fast_log to dump
 * @param scratch	a fast_log allocated with fast_log_init. Its contents
 *			will be overwritten with the contents of fb during the
 *			dumping process.
 *
 * @return		0 on success; error code otherwise
 */
extern int fast_log_dump(const struct fast_log_buf* fb,
                struct fast_log_buf* scratch, int fd);

/** Dump all fast_logs
 *
 * @param scratch	a fast_log allocated with fast_log_init. Its
 *			contents will be overwritten with the contents of
 *			fast_logs during the dumping process.
 *
 * @return		0 on success; error code otherwise
 */
extern int fast_log_dump_all(struct fast_log_buf* scratch, int fd);

#endif
