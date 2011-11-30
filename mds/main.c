/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/config/logc.h"
#include "core/config/mdsc.h"
#include "core/config/unitaryc.h"
#include "core/glitch_log.h"
#include "core/process_ctx.h"
#include "jorm/json.h"
#include "mds/net.h"
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
		int *ident, const char **config_file)
{
	int c;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	while ((c = getopt(argc, argv, "c:fhk:")) != -1) {
		switch (c) {
		case 'c':
			*config_file = optarg;
			break;
		case 'f':
			*daemonize = 0;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
		case 'k':
			str_to_int(optarg, 10, ident, err, err_len);
			if (err[0]) {
				glitch_log("Error parsing identity: %s\n", err);
				usage(EXIT_FAILURE);
			}
			break;
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
	if (*ident == 0) {
		glitch_log("0 is not a valid identity\n");
		usage(EXIT_FAILURE);
	}
	if (*ident < 0) {
		glitch_log("You must supply an identity with -k\n");
		usage(EXIT_FAILURE);
	}
}

static struct mdsc *get_mds_conf(struct unitaryc *conf, int ident)
{
	int i;
	struct mdsc **m;
	
	m = conf->mds;
	for (i = 1; (i < ident) && (*m); ++i, m++) {
		;
	}
	return *m;
}

int main(int argc, char **argv)
{
	int ret, ident = -1, daemonize = 1;
	const char *cfname = NULL;
	struct unitaryc *conf;
	struct mdsc *mconf;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	parse_argv(argc, argv, &daemonize, &ident, &cfname);
	conf = parse_unitary_conf_file(cfname, err, err_len);
	if (err[0]) {
		glitch_log("%s\n", err);
		return EXIT_FAILURE;
	}
	mconf = get_mds_conf(conf, ident);
	if (!mconf) {
		glitch_log("no MDS found in config file with id %d\n", ident);
		return EXIT_FAILURE;
	}
	if (process_ctx_init(argv[0], daemonize, mconf->lc))
		return EXIT_FAILURE;
	ret = mds_main_loop();
	process_ctx_shutdown();
	free_unitary_conf_file(conf);
	return ret;
}
