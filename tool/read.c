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

int fishtool_read(struct fishtool_params *params)
{
	const char *path, *local;
	int fd, res, ret;
	struct redfish_client *cli = NULL;
	struct redfish_file *ofe = NULL;

	path = params->non_option_args[0];
	if (!path) {
		fprintf(stderr, "fishtool_read: you must give a path name "
			"to create. -h for help.\n");
		ret = -EINVAL;
		goto done;
	}
	local = params->lowercase_args[ALPHA_IDX('o')];
	ret = redfish_connect(params->mlocs, params->user_name, &cli);
	if (ret) {
		fprintf(stderr, "redfish_connect failed with error %d\n", ret);
		goto done;
	}
	ret = redfish_open(cli, path, &ofe);
	if (ret) {
		fprintf(stderr, "redfish_open failed with error %d\n", ret);
		goto done;
	}
	if (local) {
		fd = open(local, O_TRUNC | O_CREAT | O_WRONLY);
		if (fd < 0) {
			ret = -errno;
			fprintf(stderr, "fishtool_read: error opening "
				"'%s' for write: %d\n", local, ret);
			goto done;
		}
	}
	else {
		local = "stdout";
		fd = STDOUT_FILENO;
	}
	while (1) {
		char buf[8192];

		memset(buf, 0, sizeof(buf));
		ret = redfish_read(ofe, buf, sizeof(buf));
		if (ret < 0) {
			fprintf(stderr, "redfish_read: error reading "
				"%Zd bytes from %s: %d\n", sizeof(buf), path, ret);
			goto done;
		}
		if (ret == 0) {
			break;
		}
		ret = safe_write(fd, buf, ret);
		if (ret) {
			fprintf(stderr, "fishtool_read: error writing "
				"to %s: %d\n", local, ret);
			goto done;
		}
	}
	ret = redfish_close(ofe);
	ofe = NULL;
	if (ret) {
		/* This shouldn't happen; getting errors from close only happens
		 * with a file that's open for write.  But let's check anyway.
		 */
		fprintf(stderr, "redfish_close failed with error %d\n", ret);
		goto done;
	}
	ret = 0;
done:
	if ((fd != STDOUT_FILENO) && (fd > 0)) {
		RETRY_ON_EINTR(res, close(fd));
	}
	if (ofe)
		redfish_free_file(ofe);
	if (cli)
		redfish_disconnect_and_free(cli);
	return ret;
}

const char *fishtool_read_usage[] = {
	"read: read some data from an existing file.",
	"",
	"usage:",
	"read [options] <file-name>",
	"",
	"options:",
	"-o             local file to download to",
	"               If no local file is given, stdout will be used.",
	NULL,
};

struct fishtool_act g_fishtool_read = {
	.name = "read",
	.fn = fishtool_read,
	.getopt_str = "o:",
	.usage = fishtool_read_usage,
};
