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
#include "tool/tool.h"
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

static void print_block_locs(struct redfish_block_loc **blcs, int nblc)
{
	int i, j;
	struct redfish_block_loc *blc;
	struct redfish_block_host *h;
	const char *prequel;

	for (i = 0; i < nblc; ++i) {
		blc = blcs[i];
		h = blc->hosts;

		printf("starting %"PRId64" (len:%" PRId64 "):",
				blc->start, blc->len);
		prequel = "";
		for (j = 0; j < blc->nhosts; ++j) {
			printf("%s%s:%d", prequel, h[j].hostname,  h[j].port);
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
	int ret, nblc = 0;
	uint64_t start = 0, len = 0xffffffffffffffffll;
	struct redfish_client *cli = NULL;
	struct redfish_block_loc **blcs = NULL;

	path = params->non_option_args[0];
	if (!path) {
		fprintf(stderr, "fishtool_locate: you must give a path name "
			"to create. -h for help.\n");
		ret = -EINVAL;
		goto done;
	}
	start_str = params->lowercase_args[ALPHA_IDX('s')];
	if (start_str) {
		start = str_to_u64(start_str, err, err_len);
		if (err[0]) {
			fprintf(stderr, "fishtool_locate: error parsing -s: "
				"%s\n", err);
			ret = -EINVAL;
			goto done;
		}
	}
	len_str = params->lowercase_args[ALPHA_IDX('l')];
	if (len_str) {
		len = str_to_u64(len_str, err, err_len);
		if (err[0]) {
			fprintf(stderr, "fishtool_locate: error parsing -l: "
				"%s\n", err);
			ret = -EINVAL;
			goto done;
		}
	}
	cli = redfish_connect(params->cpath, params->user_name,
		redfish_log_to_stderr, NULL, err, err_len);
	if (err[0]) {
		fprintf(stderr, "redfish_connect: failed to connect: "
				"%s\n", err);
		ret = -EIO;
		goto done;
	}
	nblc = redfish_locate(cli, path, start, len, &blcs);
	if (nblc < 0) {
		ret = nblc;
		fprintf(stderr, "redfish_locate failed with error %d\n", ret);
		goto done;
	}
	print_block_locs(blcs, nblc);
	ret = 0;
done:
	if (blcs)
		redfish_free_block_locs(blcs, nblc);
	if (cli)
		redfish_disconnect_and_release(cli);
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
