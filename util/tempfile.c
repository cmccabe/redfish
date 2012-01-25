/*
 * Copyright 2011-2012 the Redfish authors
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
		remove_tempdir(g_tempdirs[i]);
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

void unregister_tempdir_for_cleanup(const char *tempdir)
{
	int i;
	char **tempdirs;
	pthread_mutex_lock(&tempdir_lock);
	if (g_num_tempdirs == 0) {
		pthread_mutex_unlock(&tempdir_lock);
		return;
	}
	for (i = 0; i < g_num_tempdirs; ++i) {
		if (strcmp(g_tempdirs[i], tempdir) == 0)
			break;
	}
	if (i == g_num_tempdirs) {
		pthread_mutex_unlock(&tempdir_lock);
		return;
	}
	free(g_tempdirs[i]);
	g_tempdirs[i] = g_tempdirs[g_num_tempdirs - 1];
	tempdirs = realloc(g_tempdirs, sizeof(char*) * g_num_tempdirs - 1);
	if (tempdirs) {
		g_tempdirs = tempdirs;
	}
	g_num_tempdirs--;
	pthread_mutex_unlock(&tempdir_lock);
}

void remove_tempdir(const char *tempdir)
{
	run_cmd("rm", "-rf", tempdir, (char*)NULL);
}
