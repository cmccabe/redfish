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

int fishtool_chmod(struct fishtool_params *params)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	const char *path, *mode_str;
	int ret, mode = 0;
	struct redfish_client *cli = NULL;

	path = params->non_option_args[0];
	if (!path) {
		fprintf(stderr, "fishtool_chmod: you must give a path name "
			"to create. -h for help.\n");
		ret = -EINVAL;
		goto done;
	}
	mode_str = params->non_option_args[1];
	if (!mode_str) {
		fprintf(stderr, "fishtool_chmod: you must give new "
			"permission bits (in octal)\n");
		ret = -EINVAL;
		goto done;
	}
	str_to_int(mode_str, 8, &mode, err, err_len);
	if (err[0]) {
		fprintf(stderr, "fishtool_write: error parsing -p: "
			"%s\n", err);
		ret = -EINVAL;
		goto done;
	}
	cli = redfish_connect(params->cpath, params->user_name,
		redfish_log_to_stderr, NULL, err, err_len);
	if (err[0]) {
		fprintf(stderr, "redfish_connect: failed to connect: "
				"%s\n", err);
		ret = -EIO;
		goto done;
	}
	ret = redfish_chmod(cli, path, mode);
	if (ret) {
		fprintf(stderr, "redfish_chmod failed with error %d\n", ret);
		goto done;
	}
	ret = 0;
done:
	if (cli)
		redfish_disconnect_and_release(cli);
	return ret;
}

const char *fishtool_chmod_usage[] = {
	"chmod: change the permissions of a file or directory.",
	"",
	"usage:",
	"chmod <path-name> <new-permissions>",
	NULL,
};

struct fishtool_act g_fishtool_chmod = {
	.name = "chmod",
	.fn = fishtool_chmod,
	.getopt_str = "",
	.usage = fishtool_chmod_usage,
};
