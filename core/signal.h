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

#ifndef REDFISH_CORE_SIGNAL_DOT_H
#define REDFISH_CORE_SIGNAL_DOT_H

#include <unistd.h> /* for size_t */

struct logc;

typedef void (*signal_cb_t)(int);

/** The global fast log manager */
extern struct fast_log_mgr *g_fast_log_mgr;

/** Install the signal handlers for a Redfish daemon.
 *
 * @param err			a buffer to write any errors to
 * @param err_len		length of the error buffer
 * @param lc			The log config. If crash_log_path is configured,
 *				we will open a file that will be written to when
 *				a fatal signal happens.  @param fatal_signal_cb
 *				Callback that is executed after a fatal signal,
 *				or NULL for none.
 *
 * We write out an error message to error if signal_init fails.
 */
extern void signal_init(const char *argv0, char *err, size_t err_len,
		const struct logc *lc, signal_cb_t fatal_signal_cb);

/** Clear all signal handlers, free the alternate signal stack, and disable the
 * crash log.
 */
extern void signal_shutdown(void);

#endif
