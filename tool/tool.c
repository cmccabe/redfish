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

#include "client/fishc.h"
#include "core/process_ctx.h"
#include "mds/limits.h"
#include "tool/tool.h"
#include "util/string.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

struct fishtool_act g_fishtool_chmod;
struct fishtool_act g_fishtool_chown;
struct fishtool_act g_fishtool_locate;
struct fishtool_act g_fishtool_mkdirs;
struct fishtool_act g_fishtool_ping;
struct fishtool_act g_fishtool_read;
struct fishtool_act g_fishtool_rename;
struct fishtool_act g_fishtool_unlink;
struct fishtool_act g_fishtool_write;

const struct fishtool_act *g_fishtool_acts[] = {
	&g_fishtool_chmod,
	&g_fishtool_chown,
	&g_fishtool_locate,
	&g_fishtool_mkdirs,
	&g_fishtool_ping,
	&g_fishtool_read,
	&g_fishtool_rename,
	&g_fishtool_unlink,
	&g_fishtool_write,
	NULL,
};

static void fishtool_top_level_usage(int exitstatus)
{
	const struct fishtool_act **act;
	const char *prequel = "";
	static const char *usage_lines[] = {
"fishtool: the Redfish administrative tool.",
"See http://www.club.cc.cmu.edu/~cmccabe/redfish.html for the most up-to-date",
"information about Redfish.",
"",
"Standard environment variables:",
"REDFISH_MLOCS: You can set this to a comma-separated list of metadata sever ",
"               locations. Metadata sever locations are given as ",
"               <hostname>:<port>. You can also specify metadata sever ",
"               locations with -m.",
"REDFISH_USER:  You can set this to a Redfish username, which will be used as ",
"               the default Redfish username.",
"",
"Standard command-line options:",
"-h",
"    Show this help message",
"-m <hostname>:<port>",
"    Add metadata server location.",
"-u <username>",
"    Set the Redfish username to connect as.",
"",
"Fishtool commands:",
NULL
	};
	print_lines(stderr, usage_lines);
	for (act = g_fishtool_acts; *act; ++act) {
		fprintf(stderr, "%s%s", prequel, (*act)->name);
		prequel = ", ";
	}
	fprintf(stderr, "\n\n");
	fprintf(stderr, "\
For more information about each command, try\n");
	fprintf(stderr, "\
fishtool <command> -h\n");
	exit(exitstatus);
}

static void fishtool_act_usage(const struct fishtool_act *act)
{
	static const char *usage_lines[] = {
"fishtool: the Redfish administrative tool.",
"See http://www.club.cc.cmu.edu/~cmccabe/redfish.html for the most up-to-date",
"information about Redfish.",
NULL
	};
	print_lines(stderr, usage_lines);
	fprintf(stderr, "\n");
	print_lines(stderr, act->usage);
	exit(EXIT_SUCCESS);
}

const struct fishtool_act *get_fishtool_act(char *argv)
{
	const struct fishtool_act **act;

	if (argv == NULL)
		return NULL;
	for (act = g_fishtool_acts; *act; ++act) {
		if (strcmp((*act)->name, argv) == 0) {
			return *act;
		}
	}
	return NULL;
}

static struct fishtool_params* fishtool_parse_argv(int argc, char **argv)
{
	char c, **a;
	const char *s;
	char err[512] = { 0 };
	size_t i, err_len = sizeof(err);
	char getopt_str[512] = "hm:u:";
	struct fishtool_params *params;
	const char *mloc_str = NULL;
	static const char TRUE[] = "true";

	params = calloc(1, sizeof(struct fishtool_params));
	if (!params) {
		fprintf(stderr, "fishtool_parse_argv: OOM\n");
		exit(EXIT_FAILURE);
	}
	mloc_str = getenv("REDFISH_META");
	params->act = get_fishtool_act(argv[1]);
	if (params->act) {
		snappend(getopt_str, sizeof(getopt_str), "%s",
			params->act->getopt_str);
	}
	while ((c = getopt(argc, argv, getopt_str)) != -1) {
		switch (c) {
		case 'h':
			if (params->act)
				fishtool_act_usage(params->act);
			else
				fishtool_top_level_usage(EXIT_SUCCESS);
			break;
		case 'm':
			mloc_str = optarg;
			break;
		case 'u':
			params->user_name = optarg;
			break;
		case '?':
			fishtool_top_level_usage(EXIT_FAILURE);
			break;
		default:
			s = (!optarg) ? TRUE : optarg;
			if ((c >= 'a') && (c <= 'z')) {
				int idx = c - 'a';
				params->lowercase_args[idx] = s;
			}
			else if ((c >= 'A') && (c <= 'Z')) {
				int idx = c - 'A';
				params->uppercase_args[idx] = s;
			}
			else {
				fprintf(stderr, "Can't parse argument "
				       "character 0x%02x\n", c);
			}
			break;
		}
	}
	if (!params->act) {
		fprintf(stderr, "You must supply a valid action.  Give -h "
			"for more help\n");
		exit(EXIT_FAILURE);
	}
	params->mlocs = redfish_mlocs_from_str(mloc_str, err, err_len);
	if (err[0]) {
		fprintf(stderr, "%s", err);
		exit(EXIT_FAILURE);
	}
	if (params->mlocs[0] == 0) {
		fprintf(stderr, "You must supply some metadata server "
		       "locations "
			"with -m or by setting the REDFISH_META "
			"environment variable.\n");
		exit(EXIT_FAILURE);
	}
	for (i = 0, a = argv + optind + 1;
			(i < MAX_NON_OPTION_ARGS - 1) && (*a);
			++a, ++i)
	{
		params->non_option_args[i] = *a;
	}
	if (params->user_name == NULL) {
		params->user_name = RF_SUPERUSER_NAME;
	}
	return params;
}

static void free_fishtool_params(struct fishtool_params *params)
{
	redfish_mlocs_free(params->mlocs);
	free(params);
}

int main(int argc, char **argv)
{
	int ret;
	struct fishtool_params* params;

	if (utility_ctx_init(argv[0]))
		return EXIT_FAILURE;
	params = fishtool_parse_argv(argc, argv);
	ret = params->act->fn(params);
	free_fishtool_params(params);
	process_ctx_shutdown();
	return ret;
}
