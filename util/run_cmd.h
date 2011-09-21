/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_RUN_CMD_H
#define ONEFISH_RUN_CMD_H

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

#endif
