/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_CORE_PROCESS_CTX_DOT_H
#define REDFISH_CORE_PROCESS_CTX_DOT_H

#include <unistd.h> /* for size_t */

struct logc;

/** The global fast log manager */
extern struct fast_log_mgr *g_fast_log_mgr;

/** Initialize the process context.
 *
 * - initializes the global fast log manager
 * - initializes the global core log
 * - daemonizes if requested
 * - create pid file if requested
 * - sets up signals
 *
 * @param argv0			program name
 * @param daemonize		nonzero to daemonize
 * @param lc			the logging configuration
 *
 * @return			0 on success; error code otherwise
 */
extern int process_ctx_init(const char *argv0, int daemonize,
			struct logc *lc);

/** Initialize the process context for a utility program
 *
 * Just a thin wrapper around process_ctx_init.
 *
 * @param argv0			program name
 *
 * @return			0 on success; error code otherwise
 */
extern int utility_ctx_init(const char *argv0);

/** Shut down the daemon context.
 */
extern void process_ctx_shutdown(void);

#endif
