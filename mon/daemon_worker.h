/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_MON_DAEMON_WORKER_DOT_H
#define ONEFISH_MON_DAEMON_WORKER_DOT_H

#include "mon/worker.h"

#include <unistd.h> /* for size_t */
#include <semaphore.h>

struct mon_cluster;
struct worker;

enum {
	WORKER_MSG_SSH,
};

extern struct worker **g_daemon_workers;

struct worker_msg_ssh {
	struct worker_msg m;
	sem_t *sem;
	const char *args[0];
};

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

/** Tell a daemon worker to execute an ssh command. Not blocking
 *
 * @param w		The daemon worker
 * @param sem		semaphore to post after executing the command
 * @param ...		NULL-terminated list of ssh arguments
 *
 * @return		0 if the command was queued to be executed
 */
int daemon_worker_ssh(struct worker *w, sem_t *sem, ...);

#endif
