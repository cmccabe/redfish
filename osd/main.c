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
#include "common/config/osdc.h"
#include "common/config/unitaryc.h"
#include "core/glitch_log.h"
#include "core/pid_file.h"
#include "core/process_ctx.h"
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
"fishosd: the Redfish object storage daemon",
"See http://www.club.cc.cmu.edu/~cmccabe/redfish.html for the most up-to-date",
"information about Redfish.",
"",
"fishosd usage:",
"-c <osd-configuration-file>",
"    Set the osd configuration file.",
"-k <osd ID>",
"    The ID number of this OSD.  This is an index into the array of ",
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
		int *oid, const char **config_file)
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
			str_to_int(optarg, 10, oid, err, err_len);
			if (err[0]) {
				glitch_log("Error parsing OSD ID: %s\n", err);
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
	if (*oid < 0) {
		glitch_log("You must specify an OSD ID with -k.\n");
		usage(EXIT_FAILURE);
	}
}

static struct osdc *get_osd_conf(struct unitaryc *conf, int oid)
{
	int i;
	struct osdc **o;

	o = conf->osd;
	for (i = 0; (i < oid) && (*o); ++i, o++) {
		;
	}
	return *o;
}

int main(int argc, char **argv)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	int ret, oid = -1, daemonize = 1;
	const char *cfname = NULL;
	struct unitaryc *conf;
	struct osdc *oconf;

	parse_argv(argc, argv, &daemonize, &oid, &cfname);
	conf = parse_unitary_conf_file(cfname, err, err_len);
	if (err[0]) {
		glitch_log("%s\n", err);
		ret = EXIT_FAILURE;
		goto done;
	}
	oconf = get_osd_conf(conf, oid);
	if (!oconf) {
		glitch_log("Failed to find OSD %d in the configuration file\n",
			oid);
		ret = EXIT_FAILURE;
		goto done;
	}
	if (process_ctx_init(argv[0], daemonize, oconf->lc)) {
		ret = EXIT_FAILURE;
		goto done;
	}
	atexit(delete_pid_file);

	osd_net_init(conf, oid, err, err_len);
	if (err[0]) {
		glitch_log("osd_net_init error: %s\n", err);
		ret = EXIT_FAILURE;
		goto done;
	}
	ret = osd_net_main_loop();
done:
	process_ctx_shutdown();
	free_unitary_conf_file(conf);
	return ret;
}
