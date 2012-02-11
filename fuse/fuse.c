/*
 * Copyright 2012 the Redfish authors
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

#define FUSE_USE_VERSION 26

#include "util/string.h"

#include <errno.h>
#include <fcntl.h>
#include <fuse/fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void fishfuse_usage(char *argv0)
{
	int help_argc = 2;
	char *help_argv[] = { argv0, "-h", NULL };
	struct fuse_args fargs = FUSE_ARGS_INIT(help_argc, help_argv);
	static const char *usage_lines[] = {
"fishfuse: the FUSE connector for Redfish.",
"See http://www.club.cc.cmu.edu/~cmccabe/redfish.html for the most up-to-date",
"information about Redfish.",
"",
"The FUSE connector allows you to access a Redfish filesystem as if it were a",
"local filesystem.",
"",
"Redfish command-line options:",
"-c <conf-file>",
"    Set the Redfish configuration file",
"-u <username>",
"    Set the Redfish username to connect as.",
"",
"FUSE usage:",
NULL
	};
	print_lines(stderr, usage_lines);
	fuse_parse_cmdline(&fargs, NULL, NULL, NULL);
	exit(EXIT_FAILURE); /* Should be unreachable.  FUSE exits after printing
				usage information. */
}

static void fishfuse_parse_argv(int argc, char **argv,
		const char **cpath, const char **user)
{
	char c;

	opterr = 0; /* Turn off getopt error messages to stderr for unknown
			options.  They could be meaningful to FUSE. */
	*cpath = getenv("REDFISH_CONF");
	while ((c = getopt(argc, argv, "c:hu:")) != -1) {
		switch (c) {
		case 'c':
			*cpath = optarg;
			break;
		case 'h':
			fishfuse_usage(argv[0]);
			break;
		case 'u':
			*user = optarg;
			break;
		case '?':
			/* Ignore unknown options. */
			break;
		default:
			fishfuse_usage(argv[0]);
			break;
		}
	}
	if (!*cpath) {
		fprintf(stderr, "fishfuse: you must supply a Redfish "
			"configuration file path with -c or the REDFISH_CONF "
			"environment variable.  -h for help.\n");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[])
{
	const char *cpath = NULL, *user = NULL;
	struct fuse_args args;
	int err = -1;

	fishfuse_parse_argv(argc, argv, &cpath, &user);
	args.argc = argc;
	args.argv = argv;
	args.allocated = 0;

	return err ? 1 : 0;
}
