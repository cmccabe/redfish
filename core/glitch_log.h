/*
 * Copyright 2011-2012 the RedFish authors
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

#ifndef REDFISH_CORE_GLITCH_LOG_DOT_H
#define REDFISH_CORE_GLITCH_LOG_DOT_H

#include "util/compiler.h"

#include <unistd.h> /* for size_t */

struct logc;

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
void configure_glitch_log(const struct logc *lc);

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

#define GL_EXPECT_ZERO(expr) \
	do { \
		int __e__ = expr; \
		if (__e__) { \
			glitch_log(__FILE__ ": error %d on line " \
				   TO_STR2(__LINE__) "\n", __e__); \
			return __e__; \
		} \
	} while(0);

#endif
