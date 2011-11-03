/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/daemon.h"
#include "core/glitch_log.h"
#include "core/log_config.h"
#include "core/process_ctx.h"
#include "jorm/json.h"
#include "mds/net.h"
#include "util/string.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(int exitstatus)
{
	static const char *usage_lines[] = {
"fishmds: the RedFish metadata server",
"See http://www.club.cc.cmu.edu/~cmccabe/redfish.html for the most up-to-date",
"information about RedFish.",
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
			glitch_log("error parsing options.\n\n");
			usage(EXIT_FAILURE);
		}
	}
	if (argv[optind]) {
		glitch_log("junk at end of command line\n");
		usage(EXIT_FAILURE);
	}
	if (*config_file == NULL) {
		glitch_log("You must specify an mds configuration "
			"file with -c.\n\n");
		usage(EXIT_FAILURE);
	}
}

static struct daemon* parse_mds_config(const char *file_name)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct daemon *d;
	struct json_object* jo;
	
	jo = parse_json_file(file_name, err, err_len);
	if (err[0]) {
		glitch_log("error parsing json file: %s\n", err);
		return NULL;
	}
	d = JORM_FROMJSON_daemon(jo);
	json_object_put(jo);
	if (!d) {
		glitch_log("ran out of memory reading config file.\n");
		return NULL;
	}
	return d;
}

int main(int argc, char **argv)
{
	int ret, daemonize = 1;
	const char *config_file = NULL;
	struct daemon *d;

	parse_argv(argc, argv, &daemonize, &config_file);
	d = parse_mds_config(config_file);
	if (!d) {
		ret = EXIT_FAILURE;
		goto done;
	}
	if (process_ctx_init(argv[0], daemonize, d->lc)) {
		ret = EXIT_FAILURE;
		goto done;
	}
	ret = mds_main_loop();
done:
	process_ctx_shutdown();
	JORM_FREE_daemon(d);
	return ret;
}
