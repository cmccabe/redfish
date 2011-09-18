/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/run_cmd.h"
#include "util/tempfile.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Globals
static int g_tempdir_nonce = 0;

static int g_num_tempdirs = 0;

static char **g_tempdirs = NULL;

static pthread_mutex_t tempdir_lock = PTHREAD_MUTEX_INITIALIZER;

// Functions
static void cleanup_registered_tempdirs(void)
{
	int i;
	const char *skip_cleanup;
	skip_cleanup = getenv("SKIP_CLEANUP");
	if (skip_cleanup)
		return;
	pthread_mutex_lock(&tempdir_lock);
	for (i = 0; i < g_num_tempdirs; ++i) {
		run_cmd("rm", "-rf", g_tempdirs[i], (char*)NULL);
		free(g_tempdirs[i]);
	}
	free(g_tempdirs);
	g_tempdirs = NULL;
	pthread_mutex_unlock(&tempdir_lock);
}

int get_tempdir(char *tempdir, int name_max, int mode)
{
	char tmp[PATH_MAX];
	int nonce, pid;
	const char *base = getenv("TMPDIR");
	if (!base)
		base = "/tmp";
	if (base[0] != '/') {
		// canonicalize non-abosolute TMPDIR
		if (realpath(base, tmp) == NULL) {
			return -errno;
		}
		base = tmp;
	}
	pthread_mutex_lock(&tempdir_lock);
	nonce = g_tempdir_nonce++;
	pthread_mutex_unlock(&tempdir_lock);
	pid = getpid();
	snprintf(tempdir, name_max, "%s/tempdir.%08d.%08d", base, pid, nonce);
	if (mkdir(tempdir, mode) == -1) {
		int ret = errno;
		return -ret;
	}
	return 0;
}

int register_tempdir_for_cleanup(const char *tempdir)
{
	char **tempdirs;
	pthread_mutex_lock(&tempdir_lock);
	tempdirs = realloc(g_tempdirs, sizeof(char*) * (g_num_tempdirs + 1));
	if (!tempdirs)
		return -ENOMEM;
	g_tempdirs = tempdirs;
	g_tempdirs[g_num_tempdirs] = strdup(tempdir);
	if (g_num_tempdirs == 0)
		atexit(cleanup_registered_tempdirs);
	g_num_tempdirs++;
	pthread_mutex_unlock(&tempdir_lock);
	return 0;
}
