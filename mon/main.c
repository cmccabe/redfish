/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/glitch_log.h"
#include "core/log_config.h"
#include "core/signal.h"
#include "jorm/json.h"
#include "mon/action.h"
#include "mon/mon_config.h"
#include "mon/output_worker.h"
#include "mon/worker.h"
#include "util/compiler.h"
#include "util/dir.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(int exitstatus)
{
	static const char *usage_lines[] = {
"fishmon: the OneFish cluster monitoring and testing tool.",
"See http://www.club.cc.cmu.edu/~cmccabe/onefish.html for the most up-to-date",
"information about OneFish.",
"",
"fishmon usage:",
"-c <monitor-configuration-file>",
"    Set the monitor configuration file. This file contains information ",
"    about the OneFish cluster we're going to be interacting with.",
"-f",
"    Run in the foreground (do not daemonize)",
"-h",
"    Show this help message",
"-o <output-sink>",
"    Set the output sink. Valid output sinks are:",
"        none: no output at all",
"        -: output raw JSON to stdout",
"        top: show pretty output using fishtop",
"-T",
"    Show test descriptions",
NULL
	};
	print_lines(stderr, usage_lines);
	print_action_descriptions(MON_ACTION_ADMIN);
	exit(exitstatus);
}

static enum output_worker_sink_t parse_output_worker_sink(const char *str)
{
	if (strcmp(str, "none") == 0)
		return MON_OUTPUT_SINK_NONE;
	else if (strcmp(str, "-") == 0)
		return MON_OUTPUT_SINK_STDOUT;
	else if (strcmp(str, "top") == 0)
		return MON_OUTPUT_SINK_FISHTOP;
	else
		return MON_OUTPUT_SINK_NUM;
}

static void parse_arguments(int argc, char **argv, int *daemonize,
	const char **mon_config_file,
	const struct mon_action ***mon_actions,
	struct mon_action_args ***mon_args, enum output_worker_sink_t *sink)
{
	int c;
	char err[512] = { 0 };

	while ((c = getopt(argc, argv, "c:fho:T")) != -1) {
		switch (c) {
		case 'c':
			*mon_config_file = optarg;
			break;
		case 'f':
			*daemonize = 0;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
		case 'o':
			*sink = parse_output_worker_sink(optarg);
			if (*sink == MON_OUTPUT_SINK_NUM) {
				fprintf(stderr, "Invalid argument to -o.\n\n");
				usage(EXIT_FAILURE);
			}
			break;
		case 'T':
			print_action_descriptions(MON_ACTION_TEST);
			exit(EXIT_SUCCESS);
		case '?':
			fprintf(stderr, "error parsing options.\n\n");
			usage(EXIT_FAILURE);
		}
	}
	parse_mon_actions(argv + optind, err, sizeof(err),
			  mon_actions, mon_args);
	if (err[0]) {
		fprintf(stderr, "%s\n\n", err);
		usage(EXIT_FAILURE);
	}
	if (*mon_config_file == NULL) {
		fprintf(stderr, "You must specify a monitor configuration "
			"file with -c.\n\n");
		usage(EXIT_FAILURE);
	}
}

static int output_mon_config(struct json_object *jo)
{
	struct output_worker_msg *omsg =
		calloc(1, sizeof(struct output_worker_msg));
	if (!omsg)
		return -ENOMEM;
	omsg->msg.ty = WORKER_MSG_OUTPUT_JSON;
	omsg->json_ty = MON_OUTPUT_MSG_MON_CLUSTER;
	omsg->jo = jo;
	json_object_get(jo); /* output_worker will call 'put' on the conf */
	return output_worker_sendmsg_or_free(g_output_worker, omsg);
}

static int output_msg_end(void)
{
	struct json_object *jo = json_object_new_object();
	struct output_worker_msg *omsg =
		calloc(1, sizeof(struct output_worker_msg));
	if (!omsg)
		return -ENOMEM;
	omsg->msg.ty = WORKER_MSG_OUTPUT_JSON;
	omsg->json_ty = MON_OUTPUT_MSG_END;
	omsg->jo = jo;
	return output_worker_sendmsg_or_free(g_output_worker, omsg);
}

int main(int argc, char **argv)
{
	char err[512] = { 0 };
	int daemonize = 1, ret;
	size_t cur_act;
	struct json_object* jo;
	const char *mon_config_file = NULL;
	const struct mon_action **mon_actions = NULL;
	struct mon_action_args **mon_args = NULL;
	enum output_worker_sink_t sink = MON_OUTPUT_SINK_FISHTOP;
	struct mon_config *conf;
	struct log_config *lc;

	parse_arguments(argc, argv, &daemonize, &mon_config_file, &mon_actions,
			&mon_args, &sink);
	jo = parse_json_file(mon_config_file, err, sizeof(err));
	if (err[0]) {
		fprintf(stderr, "error parsing config file '%s': %s\n",
			mon_config_file, err);
		ret = EXIT_FAILURE;
		goto done;
	}
	conf = JORM_FROMJSON_mon_config(jo);
	if (!conf) {
		fprintf(stderr, "ran out of memory reading config file.\n");
		ret = EXIT_FAILURE;
		goto done_release_config;
	}
	lc = create_log_config(jo, err, sizeof(err), ONEFISH_DAEMON_TYPE_MON);
	if (err[0]) {
		fprintf(stderr, "log_config error: %s", err);
		ret = EXIT_FAILURE;
		goto done_release_config;
	}
	if (lc->base_dir) {
		do_mkdir(lc->base_dir, 0755, err, sizeof(err));
		if (err[0]) {
			fprintf(stderr, "error: %s", err);
			ret = EXIT_FAILURE;
			goto done_release_lc;
		}
	}
	open_glitch_log(lc, err, sizeof(err));
	if (err[0]) {
		fprintf(stderr, "open_glitch_log error: %s\n", err);
		ret = EXIT_FAILURE;
		goto done_release_lc;
	}
	signal_init(err, sizeof(err), NULL, NULL);
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
	ret = worker_init();
	if (ret) {
		glitch_log("worker_init error: %d\n", ret);
		ret = EXIT_FAILURE;
		goto done_signal_reset_dispositions;
	}
	ret = output_worker_init(argv[0], sink);
	if (ret) {
		glitch_log("output_worker_init error: %d\n", ret);
		ret = EXIT_FAILURE;
		goto done_signal_reset_dispositions;
	}
	ret = output_mon_config(jo);
	if (ret) {
		glitch_log("output_mon_config error: %d\n", ret);
		ret = EXIT_FAILURE;
		goto done_signal_reset_dispositions;
	}
//	mon_daemon_init(jlocal, josd, jmds, err, sizeof(err));
//	if (err[0]) {
//		glitch_log("mon_daemon_init error: %s\n", err);
//		ret = EXIT_FAILURE;
//		goto done_release_config;
//	}
	/* execute monitor actions */
	for (cur_act = 0; mon_actions[cur_act]; ++cur_act) {
		ret = mon_actions[cur_act]->fn(mon_args[cur_act]);
		if (ret) {
			goto done_shutdown_output_worker;
		}
	}
	ret = output_msg_end();
	if (ret) {
		glitch_log("output_msg_end error: %d\n", ret);
		ret = EXIT_FAILURE;
		goto done_shutdown_output_worker;
	}
	ret = EXIT_SUCCESS;

done_shutdown_output_worker:
	output_worker_shutdown();
done_signal_reset_dispositions:
	signal_resset_dispositions();
done_close_glitchlog:
	close_glitch_log();
done_release_lc:
	free_log_config(lc);
done_release_config:
	json_object_put(jo);
done:
	return ret;
}
