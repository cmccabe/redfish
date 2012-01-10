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

#ifndef REDFISH_RUN_CMD_H
#define REDFISH_RUN_CMD_H

#include <unistd.h> /* for size_t */

#define RUN_CMD_INTERNAL_ERROR 0x7ffffffd
#define RUN_CMD_EXITED_ON_SIGNAL 0x7ffffffe

/** Run a command
 *
 * @param cmd		The command to run
 * @param ...		NULL-terminated va_list of string command components
 *
 * @returns  		0 on success; error code otherwise
 */
int run_cmd(const char *cmd, ...);

/** Run a command and capture the output (both stderr and stdout)
 *
 * @param out		The buffer to write stderr/stdout to
 *			The buffer will be null-terminated.
 * @param out_len	Length of the out buffer
 * @param cvec		NULL-terminated array of command elements
 *
 * @returns		0 on success; error code otherwise
 */
int run_cmd_get_output(char *out, int out_len, const char **cvec);

/** Start a command whose stdin is hooked to a file descriptor.
 *
 * @param cvec		NULL-terminated array of command elements
 * @param pid		(out param) the process ID we created
 *
 * @returns		A file descriptor hooked to the new process' stdin, or
 * 			a negative error code
 */
int start_cmd_give_input(const char **cvec, int *pid);

/** Get a path to a file located in the same directory as this executable.
 *
 * @param argv0		argv[0]
 * @param other		Name of the other file we're looking for.
 * @param path		path output buffer
 * @param path_len	length of path output buffer
 *
 * @returns		0 on success; error code otherwise
 */
int get_colocated_path(const char *argv0, const char *other,
			   char *path, size_t path_len);

/** A thin wrapper around waitpid.
 *
 * @param pid		pid to wait for
 *
 * @return		RUN_CMD_INTERNAL_ERROR on waitpid error
 *			RUN_CMD_EXITED_ON_SIGNAL if process exited on a signal
 *			The proecss return code otherwise.
 *			Return codes are only from -128 to 127, so it is
 *			possible to distinguish the special codes from normal
 *			returns.
 */
int do_waitpid(int pid);

/** Escape a string so that the shell won't interpolate it. Uses single quotes.
 *
 * It's preferrable to avoid shell interpolation entirely than to rely on this
 * function. But sometimes that is very hard to do.
 *
 * @param src		source string
 * @param dst		(out param) destination string
 * @param dst_len	length of dst
 *
 * @return		0 on success; -ENAMETOOLONG if the input was too long
 *			for the output buffer.
 */
int shell_escape(const char *src, char *dst, size_t dst_len);

#endif
