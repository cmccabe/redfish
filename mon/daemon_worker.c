/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "mon/mon_config.h"
#include "mon/worker.h"
#include "util/compiler.h"

#include <stdlib.h>
#include <stdio.h>

struct worker **g_daemon_workers;

static int handle_daemon_worker_msg(POSSIBLY_UNUSED(struct worker_msg *m), POSSIBLY_UNUSED(void *data))
{
	//struct mon_daemon* md = (struct mon_daemon*)data;
	return 0;
}

static void free_mon_daemon(void *data)
{
	struct mon_daemon* md = (struct mon_daemon*)data;
	JORM_FREE_mon_daemon(md);
}

void init_daemon_workers(struct mon_cluster *cluster, char *err, size_t err_len)
{
	struct mon_daemon *md = NULL;
	int idx = 0, num_d;
	for (num_d = 0; cluster->daemons[num_d]; ++num_d) {
		;
	}
	g_daemon_workers = calloc(num_d + 1, sizeof(struct worker*));
	if (!g_daemon_workers)
		goto oom_error;
	for (idx = 0; idx < num_d; ++idx) {
		char name[WORKER_NAME_MAX];
		snprintf(name, WORKER_NAME_MAX, "daemon%04d", idx + 1);
		md = calloc(1, sizeof(struct mon_daemon));
		if (!md)
			goto oom_error;
		if (JORM_COPY_mon_daemon(cluster->defaults, md))
			goto oom_error;
		if (JORM_COPY_mon_daemon(cluster->daemons[idx], md))
			goto oom_error;
		md->idx = idx;
		g_daemon_workers[idx] = worker_start(name,
			handle_daemon_worker_msg, free_mon_daemon, md);
		if (!g_daemon_workers[idx]) {
			snprintf(err, err_len, "failed to create %s\n", name);
			goto error;
		}
		md = NULL;
	}
	return;
oom_error:
	if (md)
		JORM_FREE_mon_daemon(md);
	snprintf(err, err_len, "out of memory"); 
error:
	for (--idx; idx > 0; --idx) {
		worker_stop(g_daemon_workers[idx]);
		worker_join(g_daemon_workers[idx]);
	}
	return;
}

void shutdown_daemon_workers(void)
{
	int num_di;
	for (num_di = 0; g_daemon_workers[num_di]; ++num_di) {
		worker_stop(g_daemon_workers[num_di]);
		worker_join(g_daemon_workers[num_di]);
	}
}
