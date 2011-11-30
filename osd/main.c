/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/config/logc.h"
#include "core/config/osdc.h"
#include "core/config/unitaryc.h"
#include "core/glitch_log.h"
#include "core/pid_file.h"
#include "core/process_ctx.h"
#include "core/signal.h"
#include "jorm/json.h"
#include "osd/net.h"
#include "util/compiler.h"
#include "util/fast_log.h"
#include "util/str_to_int.h"
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
"-k <cluster-identity>",
"    The ID number of this OSD..  This is an index into the array of ",
"    OSDs in the configuration file.",
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
		int *ident, const char **config_file)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	int c;

	while ((c = getopt(argc, argv, "c:fk:h")) != -1) {
		switch (c) {
		case 'c':
			*config_file = optarg;
			break;
		case 'f':
			*daemonize = 0;
			break;
		case 'k':
			str_to_int(optarg, 10, ident, err, err_len);
			if (err[0]) {
				glitch_log("Error parsing identity: %s\n", err);
				usage(EXIT_FAILURE);
			}
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
	if (*ident == 0) {
		glitch_log("0 is not a valid OSD identity.\n");
		usage(EXIT_FAILURE);
	}
	if (*ident < 0) {
		glitch_log("You must specify an OSD identity with -k.\n");
		usage(EXIT_FAILURE);
	}
}

static struct osdc *get_osd_conf(struct unitaryc *conf, int ident)
{
	int i;
	struct osdc **o;

	o = conf->osd;
	for (i = 1; (i < ident) && (*o); ++i, o++) {
		;
	}
	return *o;
}

int main(int argc, char **argv)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	int ret, ident = -1, daemonize = 1;
	const char *cfname = NULL;
	struct unitaryc *conf;
	struct osdc *oconf;

	parse_argv(argc, argv, &daemonize, &ident, &cfname);
	conf = parse_unitary_conf_file(cfname, err, err_len);
	if (err[0]) {
		glitch_log("%s\n", err);
		ret = EXIT_FAILURE;
		goto done;
	}
	oconf = get_osd_conf(conf, ident);
	if (!oconf) {
		glitch_log("Failed to find OSD %d in the configuration file\n",
			ident);
		ret = EXIT_FAILURE;
		goto done;
	}
	if (process_ctx_init(argv[0], daemonize, oconf->lc)) {
		ret = EXIT_FAILURE;
		goto done;
	}
	ret = osd_main_loop(oconf);
done:
	process_ctx_shutdown();
	free_unitary_conf_file(conf);
	return ret;
}
