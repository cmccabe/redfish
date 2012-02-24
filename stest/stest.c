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

#include "client/fishc.h"
#include "client/fishc_internal.h"
#include "common/config/logc.h"
#include "core/env.h"
#include "core/process_ctx.h"
#include "mds/limits.h"
#include "stest/stest.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/macro.h"
#include "util/platform/readdir.h"
#include "util/safe_io.h"
#include "util/str_to_int.h"
#include "util/string.h"
#include "util/tempfile.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define STEST_DEFAULT_USER RF_SUPERUSER_NAME
#define NUM_PERCENT_DIGITS 3

static int g_daemonize = 1;

static int g_percent_fd = -1;

static FILE *g_err_fp;

static volatile int g_saw_err;

static char g_test_dir[PATH_MAX];

static void stest_usage(const char *argv0, const char **test_usage,
		int exitstatus)
{
	fprintf(stderr, "%s: a Redfish system test\n", argv0);
	{
		static const char *usage_lines[] = {
"See http://www.club.cc.cmu.edu/~cmccabe/redfish.html for the most up-to-date",
"information about Redfish.",
"",
"System test options:",
"-d <directory>",
"    Set the directory to use for test metadata.  Defaults to a temporary ",
"    directory",
"-f",
"    Run in the foreground (do not daemonize)",
"-h",
"    Show this help message",
NULL
		};
		print_lines(stderr, usage_lines);
	}
	fprintf(stderr, "\nTest information:\n");
	print_lines(stderr, test_usage);
	exit(exitstatus);
}

static void stest_parse_argv(int argc, char **argv, const char **test_usage)
{
	int ret, c;
	while ((c = getopt(argc, argv, "d:fh")) != -1) {
		switch (c) {
		case 'd':
			snprintf(g_test_dir, sizeof(g_test_dir), "%s", optarg);
			break;
		case 'f':
			g_daemonize = 0;
			break;
		case 'h':
			stest_usage(argv[0], test_usage, EXIT_SUCCESS);
			break;
		case '?':
			stest_usage(argv[0], test_usage, EXIT_FAILURE);
			break;
		}
	}
	if (!g_test_dir[0]) {
		ret = get_tempdir(g_test_dir, sizeof(g_test_dir), 0775);
		if (ret) {
			fprintf(stderr, "get_tempdir failed with error %d!\n",
				ret);
			exit(EXIT_FAILURE);
		}
		register_tempdir_for_cleanup(g_test_dir);
	}
}

void stest_get_conf_and_user(const char **conf, const char **user)
{
	*conf = getenv_or_die("REDFISH_CONF");
	*user = getenv_with_default("REDFISH_USER", STEST_DEFAULT_USER);
}

static void stest_status_files_init(void)
{
	char percent_fname[PATH_MAX], err_fname[PATH_MAX];

	if (zsnprintf(percent_fname, sizeof(percent_fname),
		      "%s/percent", g_test_dir)) {
		fprintf(stderr, "tempdir name too long!\n");
		exit(EXIT_FAILURE);
	}
	g_percent_fd = open(percent_fname, O_WRONLY | O_TRUNC | O_CREAT, 0640);
	if (g_percent_fd < 0) {
		int ret = errno;
		fprintf(stderr, "failed to open '%s': error %d\n",
			percent_fname, ret);
		exit(EXIT_FAILURE);
	}
	if (zsnprintf(err_fname, sizeof(err_fname),
		      "%s/err", g_test_dir)) {
		fprintf(stderr, "tempdir name too long!\n");
		exit(EXIT_FAILURE);
	}
	g_err_fp = fopen(err_fname, "w");
	if (g_err_fp == NULL) {
		int ret = errno;
		fprintf(stderr, "failed to open '%s': error %d\n",
			err_fname, ret);
		exit(EXIT_FAILURE);
	}
	stest_set_status(0);
}

static void stest_output_start_msg(void)
{
	int ret;
	char start_msg[512];
	if (zsnprintf(start_msg, sizeof(start_msg), "STEST_STARTED: %s\n",
		 g_test_dir)) {
		fprintf(stderr, "start_msg buffer too short!\n");
		exit(EXIT_FAILURE);
	}
	ret = safe_write(STDOUT_FILENO, start_msg, strlen(start_msg));
	if (ret) {
		fprintf(stderr, "safe_write to stdout failed with error %d",
			ret);
		exit(EXIT_FAILURE);
	}
}

void stest_init(int argc, char **argv, const char **test_usage)
{
	int ret;
	struct logc *lc;

	stest_parse_argv(argc, argv, test_usage);
	stest_status_files_init();
	stest_output_start_msg();
	lc = JORM_INIT_logc();
	if (!lc)
		abort();
	lc->base_dir = strdup(g_test_dir);
	if (!lc->base_dir)
		abort();
	ret = process_ctx_init(argv[0], g_daemonize, lc);
	if (ret) {
		fprintf(stderr, "process_ctx_init failed with error %d\n", ret);
		exit(EXIT_FAILURE);
	}
	JORM_FREE_logc(lc);
}

void stest_set_status(int pdone)
{
	int POSSIBLY_UNUSED(res);
	char buf[NUM_PERCENT_DIGITS + 1];
	if (pdone > 100)
		pdone = 100;
	else if (pdone < 0)
		pdone = 0;
	snprintf(buf, sizeof(buf), "%03d", pdone);
	res = safe_pwrite(g_percent_fd, buf, NUM_PERCENT_DIGITS, 0);
	if ((!g_daemonize) && (pdone > 0)) {
		fprintf(stderr, "percent done: %s\n", buf);
	}
}

void stest_add_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(g_err_fp, fmt, ap);
	va_end(ap);
	g_saw_err = 1;
	if (!g_daemonize) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}

int stest_finish(void)
{
	int res;

	stest_set_status(100);
	RETRY_ON_EINTR(res, close(g_percent_fd));
	g_percent_fd = -1;
	fclose(g_err_fp);
	g_err_fp = NULL;
	process_ctx_shutdown();
	return (g_saw_err) ? EXIT_FAILURE : EXIT_SUCCESS;
}

enum stest_stat_res stest_stat(struct redfish_client *cli, const char *path)
{
	int ret, is_dir;
	struct redfish_stat osa;

	ret = redfish_get_path_status(cli, path, &osa);
	if (ret == -ENOENT)
		return STEST_STAT_RES_ENOENT;
	else if (ret)
		return STEST_STAT_RES_ERR;
	is_dir = osa.is_dir;
	redfish_free_path_status(&osa);
	return is_dir ? STEST_STAT_RES_DIR : STEST_STAT_RES_FILE;
}

int stest_listdir(struct redfish_client *cli, const char *path,
		stest_listdir_fn fn, void *data)
{
	struct redfish_dir_entry *oda;
	int i, noda, ret;

	noda = redfish_list_directory(cli, path, &oda);
	if (noda < 0)
		return noda;
	for (i = 0; i < noda; ++i) {
		ret = fn(&oda[i], data);
		if (ret)
			goto done;
	}
	ret = noda;
done:
	redfish_free_dir_entries(oda, noda);
	return ret;
}

int stest_fuse_readdir(struct redfish_dirp* dp,
		stest_fuse_readdir_fn fn, void *data)
{
	struct dirent *de;
	int ret, noda = 0;

	while (1) {
		de = do_readdir(dp);
		if (!de)
			return noda;
		if (!strcmp(de->d_name, "."))
			continue;
		if (!strcmp(de->d_name, ".."))
			continue;
		ret = fn(de, data);
		if (ret)
			return ret;
		++noda;
	}
	return noda;
}
