/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/daemon.h"
#include "core/glitch_log.h"
#include "core/log_config.h"
#include "core/signal.h"
#include "jorm/json.h"
#include "util/compiler.h"
#include "util/string.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(int exitstatus)
{
	static const char *usage_lines[] = {
"fishmds: the OneFish metadata server",
"See http://www.club.cc.cmu.edu/~cmccabe/onefish.html for the most up-to-date",
"information about OneFish.",
"",
"fishmds usage:",
"-c <mds-configuration-file>",
"    Set the mds configuration file.",
"-f",
"    Run in the foreground (do not daemonize)",
"-h",
"    Show this help message",
NULL
	};
	print_lines(stderr, usage_lines);
	exit(exitstatus);
}

static void parse_argv(int argc, char **argv, int *daemonize,
		const char **config_file)
{
	int c;
	while ((c = getopt(argc, argv, "c:fh")) != -1) {
		switch (c) {
		case 'c':
			*config_file = optarg;
			break;
		case 'f':
			*daemonize = 0;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
		case '?':
			fprintf(stderr, "error parsing options.\n\n");
			usage(EXIT_FAILURE);
		}
	}
	if (argv[optind]) {
		fprintf(stderr, "junk at end of command line\n");
		usage(EXIT_FAILURE);
	}
	if (*config_file == NULL) {
		fprintf(stderr, "You must specify an mds configuration "
			"file with -c.\n\n");
		usage(EXIT_FAILURE);
	}
}

static struct daemon* parse_mds_config(const char *file,
					   char *err, size_t err_len)
{
	struct daemon *d;
	struct json_object* jo = parse_json_file(file, err, err_len);
	if (err[0])
		return NULL;
	d = JORM_FROMJSON_daemon(jo);
	json_object_put(jo);
	if (!d) {
		snprintf(err, err_len, "ran out of memory reading "
			 "config file.\n");
		return NULL;
	}
	return d;
}

static int main_loop(void)
{
	glitch_log("starting mds\n");
	sleep(10);
	return 0;
}

int main(int argc, char **argv)
{
	char err[512] = { 0 };
	int ret, daemonize = 1;
	const char *mds_config_file = NULL;
	struct daemon *d;

	parse_argv(argc, argv, &daemonize, &mds_config_file);
	d = parse_mds_config(mds_config_file, err, sizeof(err));
	if (err[0]) {
		fprintf(stderr, "error parsing monitor config file '%s': %s\n",
			mds_config_file, err);
		ret = EXIT_FAILURE;
		goto done;
	}
	harmonize_log_config(d->lc, err, sizeof(err), 1, 1);
	if (err[0]) {
		fprintf(stderr, "log_config error: %s", err);
		ret = EXIT_FAILURE;
		goto free_daemon;
	}
	open_glitch_log(d->lc, err, sizeof(err));
	if (err[0]) {
		fprintf(stderr, "open_glitch_log error: %s\n", err);
		ret = EXIT_FAILURE;
		goto free_daemon;
	}
	signal_init(err, sizeof(err), d->lc, NULL);
	if (err[0]) {
		fprintf(stderr, "signal_init error: %s\n", err);
		ret = EXIT_FAILURE;
		goto done_close_glitchlog;
	}
	if (daemonize) {
		if (daemon(0, 0) < 0) {
			ret = errno;
			fprintf(stderr, "daemon: error: %d\n", ret);
			ret = EXIT_FAILURE;
			goto done_signal_reset_dispositions;
		}
	}
	ret = main_loop();

done_signal_reset_dispositions:
	signal_resset_dispositions();
done_close_glitchlog:
	close_glitch_log();
free_daemon:
	JORM_FREE_daemon(d);
done:
	return ret;
}
