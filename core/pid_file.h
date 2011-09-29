/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_CORE_PID_FILE_DOT_H
#define ONEFISH_CORE_PID_FILE_DOT_H

struct log_config;

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
extern void create_pid_file(const struct log_config *lc,
			char *err, size_t err_len);

/** Delete the pid file, if it exists.
 *
 * This function is signal-safe.
 */
extern void delete_pid_file(void);

#endif
