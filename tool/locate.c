/*
 * Copyright 2011-2012 the RedFish authors
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
#include "tool/main.h"
#include "util/error.h"
#include "util/safe_io.h"
#include "util/str_to_int.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void print_block_locs(struct redfish_block_loc **blc)
{
	struct redfish_block_loc **b;

	for (b = blc; *b; ++b) {
		int i;
		const char *prequel = "";
		struct redfish_block_host *h = (*b)->hosts;

		printf("starting %"PRId64" (len:%" PRId64 "):",
				(*b)->start, (*b)->len);
		for (i = 0; i < (*b)->num_hosts; ++i) {
			printf("%s%s:%d", prequel, h[i].hostname,  h[i].port);
			prequel = ",";
		}
		printf("\n");
	}
}

int fishtool_locate(struct fishtool_params *params)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	const char *path, *start_str, *len_str;
	int ret;
	uint64_t start = 0, len = 0xffffffffffffffffll;
	struct redfish_client *cli = NULL;
	struct redfish_block_loc **blc = NULL;

	path = params->non_option_args[0];
	if (!path) {
		fprintf(stderr, "fishtool_locate: you must give a path name "
			"to create. -h for help.\n");
		ret = -EINVAL;
		goto done;
	}
	start_str = params->lowercase_args[ALPHA_IDX('s')];
	if (start_str) {
		long long int z;
		str_to_long_long(start_str, 8, &z, err, err_len);
		if (err[0]) {
			fprintf(stderr, "fishtool_locate: error parsing -s: "
				"%s\n", err);
			ret = -EINVAL;
			goto done;
		}
		start = z;
	}
	len_str = params->lowercase_args[ALPHA_IDX('l')];
	if (len_str) {
		long long int z;
		str_to_long_long(len_str, 8, &z, err, err_len);
		if (err[0]) {
			fprintf(stderr, "fishtool_locate: error parsing -l: "
				"%s\n", err);
			ret = -EINVAL;
			goto done;
		}
		len = z;
	}
	ret = redfish_connect(params->mlocs, params->user_name, &cli);
	if (ret) {
		fprintf(stderr, "redfish_connect failed with error %d\n", ret);
		goto done;
	}
	ret = redfish_locate(cli, path, start, len, &blc);
	if (ret) {
		fprintf(stderr, "redfish_locate failed with error %d\n", ret);
		goto done;
	}
	print_block_locs(blc);
	ret = 0;
done:
	if (blc)
		redfish_free_block_locs(blc);
	if (cli)
		redfish_disconnect_and_free(cli);
	return ret;
}

const char *fishtool_locate_usage[] = {
	"locate: find the locations of blocks in a regular file.",
	"",
	"usage:",
	"locate [options] <path-name>",
	"",
	"options:",
	"-s <start-offset>      starting offset in file",
	"-l <length>            length in file",
	NULL,
};

struct fishtool_act g_fishtool_locate = {
	.name = "locate",
	.fn = fishtool_locate,
	.getopt_str = "s:l:",
	.usage = fishtool_locate_usage,
};
