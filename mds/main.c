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

#include "common/config/logc.h"
#include "common/config/mdsc.h"
#include "common/config/unitaryc.h"
#include "core/glitch_log.h"
#include "core/pid_file.h"
#include "core/process_ctx.h"
#include "jorm/json.h"
#include "mds/net.h"
#include "util/error.h"
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
"fishmds: the Redfish metadata server",
"See http://www.club.cc.cmu.edu/~cmccabe/redfish.html for the most up-to-date",
"information about Redfish.",
"",
"fishmds usage:",
"-c <mds-configuration-file>",
"    Set the mds configuration file.",
"-f",
"    Run in the foreground (do not daemonize)",
"-k <mds-id>",
"    Set the mds ID.",
"-h",
"    Show this help message",
NULL
	};
	print_lines(stderr, usage_lines);
	exit(exitstatus);
}

static void parse_argv(int argc, char **argv, int *daemonize,
		int *mid, const char **config_file)
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
			*mid = str_to_int(optarg, err, err_len);
			if (err[0]) {
				glitch_log("Error parsing metadata server "
					"ID: %s\n", err);
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
	if (*mid < 0) {
		glitch_log("You must supply a metadata server ID "
			"for this server with -k\n");
		usage(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	struct fast_log_buf *fb = NULL;
	int ret, mid = -1, daemonize = 1;
	const char *cfname = NULL;
	struct unitaryc *conf;
	struct mdsc *mdsc;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	parse_argv(argc, argv, &daemonize, &mid, &cfname);
	conf = parse_unitary_conf_file(cfname, err, err_len);
	if (err[0]) {
		glitch_log("%s\n", err);
		return EXIT_FAILURE;
	}
	harmonize_unitary_conf(conf, err, err_len);
	if (err[0]) {
		glitch_log("config file error: %s\n", err);
		return EXIT_FAILURE;
	}
	mdsc = unitaryc_lookup_mdsc(conf, mid);
	if (!mdsc) {
		glitch_log("no MDS found in config file with id %d\n", mid);
		return EXIT_FAILURE;
	}
	if (process_ctx_init(argv[0], daemonize, mdsc->lc))
		return EXIT_FAILURE;
	atexit(delete_pid_file);
	fb = fast_log_create(g_fast_log_mgr, "mds_main");
	if (IS_ERR(fb)) {
		ret = PTR_ERR(fb);
		goto done;
	}
	mds_net_init(fb, conf, mdsc, mid);
	ret = mds_main_loop();
done:
	if (!IS_ERR(fb))
		fast_log_free(fb);
	process_ctx_shutdown();
	free_unitary_conf_file(conf);
	return ret;
}
