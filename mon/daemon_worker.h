/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_MON_DAEMON_WORKER_DOT_H
#define ONEFISH_MON_DAEMON_WORKER_DOT_H

#include <unistd.h> /* for size_t */

struct mon_cluster;
struct worker;

extern struct worker **g_daemon_workers;

/** Initialize the monitor's daemon workers
 *
 * @param cluster	The current configuration for the monitor cluster
 * @param err		Error buffer
 * @param err_len	Length of error buffer
 */
void init_daemon_workers(const struct mon_cluster *cluster, char *err, size_t err_len);

/** Shut down the monitor's daemon workers
 */
void shutdown_daemon_workers(void);

#endif
