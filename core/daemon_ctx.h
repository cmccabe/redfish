/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_CORE_DAEMON_CTX_DOT_H
#define REDFISH_CORE_DAEMON_CTX_DOT_H

#include <unistd.h> /* for size_t */

struct log_config;

/** Initialize the daemon context.
 *
 * - initializes the global fast log manager
 * - initializes the global core log
 * - daemonizes if requested
 * - create pid file if requested
 * - sets up signals
 */
int daemon_ctx_init(char *argv0, int daemonize, struct log_config *lc);

/** Shut down the daemon context.
 */
void daemon_ctx_shutdown(void);

#endif
