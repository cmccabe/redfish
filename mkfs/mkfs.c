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

#include "client/fishc.h"
#include "core/process_ctx.h"
#include "mds/limits.h"
#include "util/str_to_int.h"
#include "util/string.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static void fishmkfs_usage(int exitstatus)
{
	static const char *usage_lines[] = {
"fishmkfs: initializes a Redfish metadata server.",
"See http://www.club.cc.cmu.edu/~cmccabe/redfish.html for the most up-to-date",
"information about Redfish.",
"",
"Environment variables:",
"REDFISH_CONF:  If this is set, it will be used to find the default path to a ",
"               Redfish configuration file.",
"",
"Command-line options:",
"-c <file-name>",
"    Set the Redfish configuration file name",
"-F <filesystem-id>",
"    Set the filesystem ID as a 64-bit hexadecimal number",
"-h",
"    Show this help message",
"-m <metadata-server-id>",
"    Set the metadata server ID of this node",
"",
"Fishtool commands:",
NULL
	};
	print_lines(stderr, usage_lines);
	exit(exitstatus);
}

static void fishmkfs_parse_argv(int argc, char **argv, const char **cpath,
				uint16_t *mid, uint64_t *fsid)
{
	int i;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	long long ll;
	char c;

	*cpath = getenv("REDFISH_CONF");
	*mid = RF_INVAL_MID;
	*fsid = RF_INVAL_FSID;

	while ((c = getopt(argc, argv, "c:F:hm:")) != -1) {
		switch (c) {
		case 'c':
			*cpath = optarg;
			break;
		case 'F':
			str_to_long_long(optarg, 16, &ll, err, err_len);
			if (err[0]) {
				fprintf(stderr, "Failed to parse the argument "
					"to -i: %s\n", err);
				exit(EXIT_FAILURE);
			}
			*fsid = (uint64_t)ll;
			break;
		case 'h':
			fishmkfs_usage(EXIT_SUCCESS);
			break;
		case 'm':
			str_to_int(optarg, 10, &i, err, err_len);
			if (err[0]) {
				fprintf(stderr, "Failed to parse the argument "
					"to -m: %s\n", err);
				exit(EXIT_FAILURE);
			}
			if ((i >= RF_MAX_MDS) || (i < 0)) {
				fprintf(stderr, "Invalid MDS ID: MDS IDs must "
					"be between 0 and %d\n", RF_MAX_MDS);
				exit(EXIT_FAILURE);
			}
			*mid = (uint16_t)i;
			break;
		default:
			fishmkfs_usage(EXIT_FAILURE);
			break;
		}
	}
	if (*cpath == NULL) {
		fprintf(stderr, "You must supply a Redfish configuration "
			"file.  Give -h for more help\n");
		exit(EXIT_FAILURE);
	}
	if (*mid == RF_INVAL_MID) {
		fprintf(stderr, "You must give the metadata server ID. "
			"Give -h for more help\n");
		exit(EXIT_FAILURE);
	}
	if (*fsid == RF_INVAL_FSID) {
		fprintf(stderr, "You must give a filesystem ID. "
			"Give -h for more help\n");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	int ret;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	const char *uconf;
	uint16_t mid;
	uint64_t fsid;

	if (utility_ctx_init(argv[0]))
		return EXIT_FAILURE;
	fishmkfs_parse_argv(argc, argv, &uconf, &mid, &fsid);
	redfish_mkfs(uconf, mid, fsid, err, err_len);
	if (err[0]) {
		fprintf(stderr, "fishmkfs: mkfs failed with error: %s\n", err);
		ret = EXIT_FAILURE;
	}
	ret = 0;
	process_ctx_shutdown();
	return ret;
}
