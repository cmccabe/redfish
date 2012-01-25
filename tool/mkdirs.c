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
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int fishtool_mkdirs(struct fishtool_params *params)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	const char *path, *mode_str;
	int ret, mode = 0644;
	struct redfish_client *cli = NULL;

	mode_str = params->lowercase_args[ALPHA_IDX('p')];
	if (mode_str) {
		str_to_int(mode_str, 8, &mode, err, err_len);
		if (err[0]) {
			fprintf(stderr, "fishtool_write: error parsing -p: "
				"%s\n", err);
			ret = -EINVAL;
			goto done;
		}
	}
	path = params->non_option_args[0];
	if (!path) {
		fprintf(stderr, "fishtool_mkdirs: you must give a path name "
			"to create. -h for help.\n");
		ret = -EINVAL;
		goto done;
	}
	ret = redfish_connect(params->mlocs, params->user_name, &cli);
	if (ret) {
		fprintf(stderr, "redfish_connect failed with error %d\n", ret);
		goto done;
	}
	ret = redfish_mkdirs(cli, mode, path);
	if (ret) {
		fprintf(stderr, "redfish_mkdirs failed with error %d\n", ret);
		goto done;
	}
	ret = 0;
done:
	if (cli)
		redfish_disconnect_and_release(cli);
	return ret;
}

const char *fishtool_mkdirs_usage[] = {
	"mkdirs: create zero or more directories.",
	"",
	"usage:",
	"mkdirs [options] <path-name>",
	"",
	"options:",
	"-p <octal-num> permissions bits to use",
	NULL,
};

struct fishtool_act g_fishtool_mkdirs = {
	.name = "mkdirs",
	.fn = fishtool_mkdirs,
	.getopt_str = "p:",
	.usage = fishtool_mkdirs_usage,
};
