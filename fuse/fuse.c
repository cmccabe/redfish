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

#include "util/cram.h"
#include "util/string.h"

#include <errno.h>
#include <fcntl.h>
#include <fuse/fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct fuse_lowlevel_ops g_fishfuse_ops = {
	.read		= NULL,
};

static void xsrealloc(char **dst, const char *src)
{
	char *out;
	size_t slen;

	if (!src)
		src = "";
	slen = strlen(src);
	out = realloc(*dst, slen + 1);
	if (!out)
		abort();
	strcpy(out, src);
	*dst = out;
}

static void fishfuse_usage(char *argv0)
{
	int help_argc = 2;
	char *help_argv[] = { argv0, "-ho", NULL };
	struct fuse_args fargs = FUSE_ARGS_INIT(help_argc, help_argv);
	static const char *usage_lines[] = {
"fishfuse: the FUSE connector for Redfish.",
"See http://www.club.cc.cmu.edu/~cmccabe/redfish.html for the most up-to-date",
"information about Redfish.",
"",
"The FUSE connector allows you to access a Redfish filesystem as if it were a",
"local filesystem.",
"",
"USAGE",
"fishfuse [mount point] [options]",
"",
"EXAMPLE USAGE",
"fishfuse /mnt/tmp -o 'large_read,user=foo,conf=/etc/redfish.conf'",
"",
"GENERAL OPTIONS",
"-h, --help            This help message",
"-V                    Print FUSE version",
"",
"REDFISH OPTIONS",
"-o user=<USER>        Set the Redfish user name.",
"-o conf=<PATH>        Set the Redfish configuration file path.",
"",
NULL
	};
	print_lines(stderr, usage_lines);
	fuse_parse_cmdline(&fargs, NULL, NULL, NULL);
	exit(EXIT_FAILURE); /* Should be unreachable.  FUSE exits after printing
				usage information. */
}

#define RF_FUSE_USER_OPT "user="
#define RF_FUSE_CONF_OPT "conf="

static void fishfuse_handle_opts(const char *opts, char **cpath,
		char **user, struct fuse_args *fargs)
{
	char *buf, *tok, *state = NULL, *out = NULL;

	buf = strdup(opts);
	if (!buf)
		abort();
	for (tok = strtok_r(buf, ",", &state); tok;
			(tok = strtok_r(NULL, ";", &state))) {
		if (!strncmp(tok, RF_FUSE_USER_OPT,
				sizeof(RF_FUSE_USER_OPT) - 1)) {
			xsrealloc(user, tok + sizeof(RF_FUSE_USER_OPT) - 1);
		}
		else if (!strncmp(tok, RF_FUSE_CONF_OPT,
				sizeof(RF_FUSE_CONF_OPT) - 1)) {
			xsrealloc(cpath, tok + sizeof(RF_FUSE_CONF_OPT) - 1);
		}
		else if (!out) {
			/* sizeof("-o") includes the NULL byte */
			out = malloc(strlen(opts) + sizeof("-o"));
			if (!out)
				abort();
			strcpy(out, "-o");
			strcat(out, tok);
		}
		else {
			printf("case DD\n");
			strcat(out, ",");
			strcat(out, tok);
		}
	}
	if (out) {
		fargs->argv[fargs->argc++] = out;
	}
	free(buf);
}

static void fishfuse_parse_argv(int argc, char **argv,
		char **cpath, char **user, struct fuse_args *fargs)
{
	int idx;
	size_t olen;

	xsrealloc(cpath, getenv("REDFISH_CONF"));
	fargs->argc = 0;
	fargs->allocated = 1;
	fargs->argv = calloc(argc + 1, sizeof(char*));
	if (!fargs->argv)
		abort();
	fargs->argv[fargs->argc] = strdup(argv[0]);
	if (!fargs->argv[fargs->argc++])
		abort();
	idx = 1;
	while (1) {
		if (idx >= argc)
			break;
		olen = strlen(argv[idx]);

		if ((!strcmp(argv[idx], "-h")) ||
				(!strcmp(argv[idx], "--help"))) {
			fishfuse_usage(argv[0]);
		}
		else if ((olen >= 2) && (argv[idx][0] == '-') &&
				(argv[idx][1] == 'o')) {
			if (olen == 2) {
				if (++idx >= argc) {
					fprintf(stderr, "Missing argument "
						"after -o\n");
					exit(EXIT_FAILURE);
				}
				fishfuse_handle_opts(argv[idx++], cpath,
						user, fargs);
			}
			else {
				fishfuse_handle_opts(argv[idx++] + 2, cpath,
						user, fargs);
			}
		}
		else {
			fargs->argv[fargs->argc++] = strdup(argv[idx++]);
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
	char *cpath = NULL, *user = NULL;
	struct fuse_args fargs;
	struct fuse_chan *ch;
	char *mountpoint;
	int err = -1;

	fishfuse_parse_argv(argc, argv, &cpath, &user, &fargs);
	if (fuse_parse_cmdline(&fargs, &mountpoint, NULL, NULL) != -1 &&
			(ch = fuse_mount(mountpoint, &fargs)) != NULL) {
		struct fuse_session *se;

		se = fuse_lowlevel_new(&fargs, &g_fishfuse_ops,
				       sizeof(g_fishfuse_ops), NULL);
		if (se != NULL) {
			if (fuse_set_signal_handlers(se) != -1) {
				fuse_session_add_chan(se, ch);
				err = fuse_session_loop(se);
				fuse_remove_signal_handlers(se);
				fuse_session_remove_chan(ch);
			}
			fuse_session_destroy(se);
		}
		fuse_unmount(mountpoint, ch);
	}
	fuse_opt_free_args(&fargs);

	free(cpath);
	free(user);
	return cram_into_u8(err);
}
