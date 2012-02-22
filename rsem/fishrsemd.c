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
#include "core/glitch_log.h"
#include "core/pid_file.h"
#include "core/signal.h"
#include "jorm/json.h"
#include "rsem/rsem.h"
#include "rsem/rsem_srv.h"
#include "util/compiler.h"
#include "util/string.h"
#include "util/time.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(int exitstatus)
{
	static const char *usage_lines[] = {
"fishrsemd: the Redfish remote semaphore server",
"See http://www.club.cc.cmu.edu/~cmccabe/redfish.html for the most up-to-date",
"information about Redfish.",
"",
"fishrsemd usage:",
"-c <semaphore-configuration-file>",
"    Set the sempahore configuration file.",
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
		glitch_log("You must specify a fishrsemd configuration "
			"file with -c.\n\n");
		usage(EXIT_FAILURE);
	}
}

static struct rsem_server_conf* parse_config(const char *file,
					   char *err, size_t err_len)
{
	struct rsem_server_conf *conf;
	struct json_object* jo = parse_json_file(file, err, err_len);
	if (err[0])
		return NULL;
	conf = JORM_FROMJSON_rsem_server_conf(jo);
	json_object_put(jo);
	if (!conf) {
		snprintf(err, err_len, "ran out of memory reading "
			 "config file.\n");
		return NULL;
	}
	return conf;
}

int main(int argc, char **argv)
{
	char err[512] = { 0 };
	int ret, daemonize = 1;
	const char *config_file = NULL;
	struct rsem_server* rss;
	struct rsem_server_conf *conf;

	parse_argv(argc, argv, &daemonize, &config_file);
	conf = parse_config(config_file, err, sizeof(err));
	if (err[0]) {
		glitch_log("error parsing config file '%s': %s\n",
			config_file, err);
		ret = EXIT_FAILURE;
		goto done;
	}
	harmonize_logc(conf->lc, err, sizeof(err), 1);
	if (err[0]) {
		glitch_log("logc error: %s\n", err);
		ret = EXIT_FAILURE;
		goto free_conf;
	}
	configure_glitch_log(conf->lc);
	signal_init(argv[0], err, sizeof(err), conf->lc);
	if (err[0]) {
		glitch_log("signal_init error: %s\n", err);
		ret = EXIT_FAILURE;
		goto done_close_glitchlog;
	}
	if (daemonize) {
		if (daemon(0, 0) < 0) {
			ret = errno;
			glitch_log("daemon: error: %d\n", ret);
			ret = EXIT_FAILURE;
			goto done_signal_shutdown;
		}
	}
	create_pid_file(conf->lc, err, sizeof(err));
	if (err[0]) {
		glitch_log("create_pid_file error: %s\n", err);
		ret = EXIT_FAILURE;
		goto done_signal_shutdown;
	}
	rss = start_rsem_server(conf, err, sizeof(err));
	if (err[0]) {
		glitch_log("start_rsem_server error: %s\n", err);
		ret = EXIT_FAILURE;
		goto done_signal_shutdown;
	}
	while (1) {
		mt_msleep(100000);
	}
	rsem_server_shutdown(rss);

done_signal_shutdown:
	signal_shutdown();
done_close_glitchlog:
	close_glitch_log();
free_conf:
	JORM_FREE_rsem_server_conf(conf);
done:
	return ret;
}
