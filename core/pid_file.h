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

#ifndef REDFISH_CORE_PID_FILE_DOT_H
#define REDFISH_CORE_PID_FILE_DOT_H

struct logc;

/** Create a pid file and register it to be deleted when the program exits.
 *
 * This function should only be called once, when starting up.
 * It must be called after daemonize(), or else the pid we get will not be
 * valid.
 *
 * @param lc		The log config
 * @param err		(out param) the error buffer
 * @param err_len	length of the error buffer
 */
extern void create_pid_file(const struct logc *lc,
			char *err, size_t err_len);

/** Delete the pid file, if it exists.
 *
 * This function is signal-safe.
 */
extern void delete_pid_file(void);

#endif
