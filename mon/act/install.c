/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "mon/action.h"
#include "mon/daemon_worker.h"
#include "mon/mon_info.h"
#include "mon/worker.h"
#include "util/compiler.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

static const char *install_names[] = { "install", NULL };

static const char *install_desc[] = {
"install: install new onefish software",
"    The install action copies binaries to all configured nodes. The ",
"    binaries will be copied from the same directory as the monitor ",
"    executable.",
NULL,
};

static const char *install_args[] = { NULL };

static int do_install(struct action_info *ai,
		POSSIBLY_UNUSED(struct action_arg ** args))
{
	sem_t sem;
	int i, ret, num_daemon = 0;
	struct worker **w;

	for (w = g_daemon_workers; *w; ++w) {
		++num_daemon;
	}
	ret = sem_init(&sem, 0, num_daemon);
	if (ret) {
		return ret;
	}
	for (w = g_daemon_workers; *w; ++w) {
		daemon_worker_upload_binary(*w, &sem, NULL);
	}
	for (i = 0; i < num_daemon; ++i) {
		sem_wait(&sem);
		pthread_mutex_lock(&g_mon_info_lock);
		ai->percent_done = (i * 100) / num_daemon;
		ai->changed = 1;
		pthread_mutex_unlock(&g_mon_info_lock);
	}
	sem_destroy(&sem);

	return 0;
}

const struct mon_action install_act = {
	.ty = MON_ACTION_ADMIN,
	.names = install_names,
	.desc = install_desc,
	.args = install_args,
	.fn = do_install,
};
