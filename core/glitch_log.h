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

/** Open the glitch log. This should be called before any calls to glitch_log
 *
 * @param lc		The log configuration
 *
 * @return		0 on success; error code otherwise
 */
void open_glitch_log(const struct log_config *lc, char *err, size_t err_len);

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
 *
 * Must be called after all threads using glitch_log have been stopped.
 */
void close_glitch_log(void);

#endif
