/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/log_config.h"
#include "core/signal.h"
#include "stest/stest.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/safe_io.h"
#include "util/string.h"
#include "util/tempfile.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static char g_tempdir[PATH_MAX];

static int g_daemonize = 1;

static int g_percent_fd = -1;

static FILE *g_err_fp = NULL;

static volatile int g_saw_err = 0;

const char *copt_get(const char *key, struct stest_custom_opt *copt, int ncopt)
{
	int i;
	for (i = 0; i < ncopt; ++i) {
		if (strcmp(key, copt[i].key) == 0)
			return copt[i].val;
	}
	return NULL;
}

static void stest_usage(const char *argv0, struct stest_custom_opt *copt,
			int ncopt, int exitstatus)
{
	int i;
	fprintf(stderr, "%s: a OneFish system test\n", argv0);
	{
		static const char *usage_lines[] = {
"See http://www.club.cc.cmu.edu/~cmccabe/onefish.html for the most up-to-date",
"information about OneFish.",
"",
"Standard system test options:",
"-f",
"    Run in the foreground (do not daemonize)",
"-h",
"    Show this help message",
NULL
		};
		print_lines(stderr, usage_lines);
	}
	if (ncopt == 0)
		exit(exitstatus);
	fprintf(stderr, "\nTest-specific options:\n");
	for (i = 0; i < ncopt; ++i) {
		fprintf(stderr, "%s", copt[i].help);
	}
	fputs("\n", stderr);
	exit(exitstatus);
}

static void stest_parse_argv(int argc, char **argv,
	struct stest_custom_opt *copt, int ncopt)
{
	char **s;
	int c;
	while ((c = getopt(argc, argv, "fh")) != -1) {
		switch (c) {
		case 'f':
			g_daemonize = 0;
			break;
		case 'h':
			stest_usage(argv[0], copt, ncopt, EXIT_SUCCESS);
		case '?':
			stest_usage(argv[0], copt, ncopt, EXIT_FAILURE);
		}
	}
	for (s = argv + optind; *s; ++s) {
		int i;
		char *eq = index(*s, '=');
		size_t strlen_s = strlen(*s);
		size_t strlen_v;
		if (eq == NULL) {
			fprintf(stderr, "Error parsing custom argument %s: "
				"could not find equals sign.\n", *s);
			stest_usage(argv[0], copt, ncopt, EXIT_FAILURE);
		}
		++eq;
		strlen_v = strlen(eq);
		for (i = 0; i < ncopt; ++i) {
			if (!strncmp(copt[i].key, *s,
				     strlen_s - strlen_v - 1)) {
				break;
			}
		}
		if (i == ncopt) {
			fprintf(stderr, "Error parsing custom argument %s: "
				"it is not an option for this test.\n", *s);
			stest_usage(argv[0], copt, ncopt, EXIT_FAILURE);
		}
		copt[i].val = eq;
	}
}

static void stest_status_files_init(void)
{
	char percent_fname[PATH_MAX], err_fname[PATH_MAX];

	int ret = get_tempdir(g_tempdir, sizeof(g_tempdir), 0775);
	if (ret) {
		fprintf(stderr, "get_tempdir failed with error %d!\n", ret);
		exit(EXIT_FAILURE);
	}
	if (!g_daemonize) {
		register_tempdir_for_cleanup(g_tempdir);
	}
	if (zsnprintf(percent_fname, sizeof(percent_fname),
		      "%s/percent", g_tempdir)) {
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
		      "%s/err", g_tempdir)) {
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
		 g_tempdir)) {
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

static void stest_signal_fn(POSSIBLY_UNUSED(int signal))
{
	stest_set_status(100);
}

static void stest_init_signals(const char *argv0)
{
	char crash_log[PATH_MAX];
	char err[512] = { 0 };
	struct log_config lc;

	if (zsnprintf(crash_log, sizeof(crash_log), "%s/crash", g_tempdir)) {
		fprintf(stderr, "start_msg buffer too short!\n");
		exit(EXIT_FAILURE);
	}
	memset(&lc, 0, sizeof(lc));
	lc.crash_log = crash_log;
	signal_init(argv0, err, sizeof(err), &lc, stest_signal_fn);
	if (err[0]) {
		fprintf(stderr, "signal_init error: %s\n", err);
		exit(EXIT_FAILURE);
	}
}

void stest_init(int argc, char **argv,
	struct stest_custom_opt *copt, int ncopt)
{
	stest_parse_argv(argc, argv, copt, ncopt);
	stest_status_files_init();
	stest_init_signals(argv[0]);
	stest_output_start_msg();
	if (g_daemonize) {
		daemon(0, 0);
	}
}

#define NUM_PERCENT_DIGITS 3

void stest_set_status(int pdone)
{
	int res;
	char buf[NUM_PERCENT_DIGITS + 1];
	if (pdone > 100)
		pdone = 100;
	else if (pdone < 0)
		pdone = 0;
	snprintf(buf, sizeof(buf), "%03d", pdone);
	res = safe_pwrite(g_percent_fd, buf, NUM_PERCENT_DIGITS, 0);
	if (!g_daemonize) {
		fprintf(stderr, "percent done: %s\n", buf);
	}
}

void stest_add_error(const char *err)
{
	fprintf(g_err_fp, "%s", err);
	g_saw_err = 1;
	if (!g_daemonize) {
		fprintf(stderr, "error: %s", err);
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
	return (g_saw_err) ? EXIT_FAILURE : EXIT_SUCCESS; 
}
