/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_CORE_GLITCH_LOG_DOT_H
#define ONEFISH_CORE_GLITCH_LOG_DOT_H

#include "util/compiler.h"

#include <unistd.h> /* for size_t */

struct log_config;

/** Glitch log is a log that daemons use specifically to log glitches-- bad
 * conditions that should not ever occur. Because it's designed to log only
 * exceptional conditions, glitch log is low-performance, unlike fastlog.
 * System administrators should carefully examine anything that goes into the
 * glitch log, because each message indicates a bad condition or serious error.
 *
 * glitch_log will always output to stderr. It will output to syslog or to log
 * files if that is configured.
 *
 * You can use glitch log before configuring it. Your logs will simply go to
 * stderr. When you do get around to configuring it, the logs you have already
 * outputted will be copied to the configured location.
 */

/** Configure the glitch log.
 *
 * @param lc		The log configuration
 */
void configure_glitch_log(const struct log_config *lc);

/** Issue a glitch_log message.
 * This function is thread-safe because it uses the log_config lock.
 *
 * @param fmt		Printf-style format string
 * @param ...		Printf-style variadic arguments
 *
 * @return		0 on success; error code otherwise
 */
void glitch_log(const char *fmt, ...) PRINTF_FORMAT(1, 2);

/** Close the glitch log.
 */
void close_glitch_log(void);

#endif
