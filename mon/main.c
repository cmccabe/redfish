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
#include "mon/daemon_worker.h"
#include "mon/mon_config.h"
#include "mon/mon_info.h"
#include "mon/output_worker.h"
#include "mon/worker.h"
#include "util/compiler.h"
#include "util/dir.h"
#include "util/string.h"

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
"-T",
"    Show test descriptions",
NULL
	};
	print_lines(stderr, usage_lines);
	print_action_descriptions(MON_ACTION_ADMIN);
	exit(exitstatus);
}

static void parse_argv(int argc, char **argv, int *daemonize,
		const char **mon_config_file, struct action_info*** ai)
{
	int c;
	char err[512] = { 0 };

	while ((c = getopt(argc, argv, "c:fhT")) != -1) {
		switch (c) {
		case 'c':
			*mon_config_file = optarg;
			break;
		case 'f':
			*daemonize = 0;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
		case 'T':
			print_action_descriptions(MON_ACTION_TEST);
			exit(EXIT_SUCCESS);
		case '?':
			fprintf(stderr, "error parsing options.\n\n");
			usage(EXIT_FAILURE);
		}
	}
	*ai = argv_to_action_info(argv + optind, err, sizeof(err));
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

static int main_loop(void)
{
	int ret;
	size_t idx = 0;

	/* execute monitor actions */
	pthread_mutex_lock(&g_mon_info_lock);
	while (1) {
		const struct mon_action* act;
		struct action_info *ai;
		struct action_arg **args_copy;

		ai = g_mon_info.acts[idx];
		if (ai == NULL) {
			/* no more actions to execute. */
			pthread_mutex_unlock(&g_mon_info_lock);
			ret = EXIT_SUCCESS;
			break;
		}
		act = parse_one_action(ai->act_name);
		args_copy = JORM_ARRAY_COPY_action_arg(ai->args);
		pthread_mutex_unlock(&g_mon_info_lock);
		ret = act->fn(ai, args_copy);
		if (ret) {
			ret = EXIT_FAILURE;
			break;
		}
		JORM_ARRAY_FREE_action_arg(&args_copy);
		pthread_mutex_lock(&g_mon_info_lock);
		if ((act->ty == MON_ACTION_IDLE) &&
		    (g_mon_info.acts[idx + 1] == NULL)) {
			/* If we're on the last action, and it is an idle
			 * action, keep doing it. */
		}
		else {
			/* next action */
			++idx;
		}
	}
	return ret;
}

static struct mon_config* parse_mon_config(const char *file,
					   char *err, size_t err_len)
{
	struct mon_config *mc;
	struct json_object* jo = parse_json_file(file, err, err_len);
	if (err[0])
		return NULL;
	mc = JORM_FROMJSON_mon_config(jo);
	json_object_put(jo);
	if (!mc) {
		snprintf(err, err_len, "ran out of memory reading "
			 "config file.\n");
		return NULL;
	}
	return mc;
}

struct daemon_info** create_daemons_array(const struct mon_cluster *cluster,
					  char *err, size_t err_len)
{
	struct daemon_info** di;
	int idx, num_daemons;
	for (num_daemons = 0; cluster->daemons[num_daemons]; ++num_daemons) {
		;
	}
	di = calloc(num_daemons + 1, sizeof(struct daemon_info*));
	if (!di)
		goto oom_error;
	for (idx = 0; idx < num_daemons; ++idx) {
		di[idx] = calloc(1, sizeof(struct daemon_info));
		if (!di[idx])
			goto oom_error;
		di[idx]->idx = idx;
		if (cluster->daemons[idx]->type == NULL) {
			snprintf(err, err_len, "you must supply a 'type' "
				 "for daemon %d", idx + 1);
			goto error;
		}
		if (strcmp(cluster->daemons[idx]->type, "mds") == 0) {
			di[idx]->type = ONEFISH_DAEMON_TYPE_MDS;
		}
		else if (strcmp(cluster->daemons[idx]->type, "osd") == 0) {
			di[idx]->type = ONEFISH_DAEMON_TYPE_OSD;
		}
		else {
			snprintf(err, err_len, "error parsing type of daemon "
				 "%d: must be 'mds' or 'osd'", idx + 1);
			goto error;
		}
		di[idx]->status = NULL;
		di[idx]->changed = 0;
	}
	return di;

oom_error:
	snprintf(err, err_len, "out of memory.");
error:
	if (di)
		JORM_ARRAY_FREE_daemon_info(&di);
	return NULL;


}

int main(int argc, char **argv)
{
	char err[512] = { 0 };
	int ret, daemonize = 1;
	const char *mon_config_file = NULL;
	struct action_info** ai = NULL;
	struct mon_config *mc;

	parse_argv(argc, argv, &daemonize, &mon_config_file, &ai);
	g_mon_info.acts = argv_to_action_info(argv + optind, err, sizeof(err));
	if (err[0]) {
		fprintf(stderr, "%s", err);
		ret = EXIT_FAILURE;
		goto done;
	}
	mc = parse_mon_config(mon_config_file, err, sizeof(err));
	if (err[0]) {
		fprintf(stderr, "error parsing monitor config file '%s': %s\n",
			mon_config_file, err);
		ret = EXIT_FAILURE;
		goto free_action_info;
	}
	g_mon_info.daemons = create_daemons_array(mc->cluster,
						  err, sizeof(err));
	if (err[0]) {
		fprintf(stderr, "create_daemons_array error: %s\n", err);
		ret = EXIT_FAILURE;
		goto free_mc;
	}
	harmonize_log_config(mc->lc, err, sizeof(err), 1, 1);
	if (err[0]) {
		fprintf(stderr, "log_config error: %s", err);
		ret = EXIT_FAILURE;
		goto free_daemon_info;
	}
	open_glitch_log(mc->lc, err, sizeof(err));
	if (err[0]) {
		fprintf(stderr, "open_glitch_log error: %s\n", err);
		ret = EXIT_FAILURE;
		goto free_daemon_info;
	}
	signal_init(err, sizeof(err), mc->lc, NULL);
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
	init_daemon_workers(mc->cluster, err, sizeof(err));
	if (err[0]) {
		glitch_log("init_daemon_workers error: %s\n", err);
		goto done_signal_reset_dispositions;
	}
	init_output_worker(mc->lc->socket_path, err, sizeof(err));
	if (err[0]) {
		glitch_log("error initializing output worker: %d\n", ret);
		ret = EXIT_FAILURE;
		goto done_shutdown_daemon_workers;
	}
	ret = main_loop();

	shutdown_output_worker();
done_shutdown_daemon_workers:
	shutdown_daemon_workers();
done_signal_reset_dispositions:
	signal_resset_dispositions();
done_close_glitchlog:
	close_glitch_log();
free_daemon_info:
	JORM_ARRAY_FREE_daemon_info(&g_mon_info.daemons);
free_mc:
	JORM_FREE_mon_config(mc);
free_action_info:
	JORM_ARRAY_FREE_action_info(&g_mon_info.acts);
done:
	return ret;
}
