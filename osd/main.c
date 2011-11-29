/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/config/logc.h"
#include "core/config/osdc.h"
#include "core/glitch_log.h"
#include "core/pid_file.h"
#include "core/process_ctx.h"
#include "core/signal.h"
#include "jorm/json.h"
#include "osd/net.h"
#include "util/compiler.h"
#include "util/fast_log.h"
#include "util/string.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(int exitstatus)
{
	static const char *usage_lines[] = {
"fishosd: the RedFish object storage daemon",
"See http://www.club.cc.cmu.edu/~cmccabe/redfish.html for the most up-to-date",
"information about RedFish.",
"",
"fishosd usage:",
"-c <osd-configuration-file>",
"    Set the osd configuration file.",
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
		glitch_log("You must specify an osd configuration "
			"file with -c.\n\n");
		usage(EXIT_FAILURE);
	}
}

static struct osdc* parse_osdc(const char *file_name)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct osdc *conf;
	struct json_object* jo;
	
	jo = parse_json_file(file_name, err, err_len);
	if (err[0]) {
		glitch_log("error parsing json file: %s\n", err);
		return NULL;
	}
	conf = JORM_FROMJSON_osdc(jo);
	json_object_put(jo);
	if (!conf) {
		glitch_log("ran out of memory reading config file.\n");
		return NULL;
	}
	return conf;
}

int main(int argc, char **argv)
{
	int ret, daemonize = 1;
	const char *osdc_file = NULL;
	struct osdc *conf;

	parse_argv(argc, argv, &daemonize, &osdc_file);
	conf = parse_osdc(osdc_file);
	if (!conf) {
		ret = EXIT_FAILURE;
		goto done;
	}
	if (process_ctx_init(argv[0], daemonize, conf->lc)) {
		ret = EXIT_FAILURE;
		goto done;
	}
	ret = osd_main_loop(conf);
done:
	process_ctx_shutdown();
	JORM_FREE_osdc(conf);
	return ret;
}
