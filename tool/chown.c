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
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int fishtool_chown(struct fishtool_params *params)
{
	const char *path, *user, *group;
	int ret;
	struct redfish_client *cli = NULL;

	user = params->uppercase_args[ALPHA_IDX('U')];
	group = params->lowercase_args[ALPHA_IDX('g')];
	if ((user == NULL) && (group == NULL)) {
		fprintf(stderr, "fishtool_chown: you should specify a new "
			"user or new group, or both, to set.\n");
		ret = -EINVAL;
		goto done;
	}
	path = params->non_option_args[0];
	if (!path) {
		fprintf(stderr, "fishtool_chown: you must give a path name "
			"to chown. -h for help.\n");
		ret = -EINVAL;
		goto done;
	}
	ret = redfish_connect(params->mlocs, params->user_name, &cli);
	if (ret) {
		fprintf(stderr, "redfish_connect failed with error %d\n", ret);
		goto done;
	}
	ret = redfish_chown(cli, path, user, group);
	if (ret) {
		fprintf(stderr, "redfish_chown failed with error %d\n", ret);
		goto done;
	}
	ret = 0;
done:
	if (cli)
		redfish_disconnect_and_free(cli);
	return ret;
}

const char *fishtool_chown_usage[] = {
	"chown: change the ownership of a file or directory.",
	"",
	"usage:",
	"chown [options] <path-name>",
	"",
	"options:",
	"-g <group-name>        New group to set",
	"-U <user-name>         New owner to set",
	NULL,
};

struct fishtool_act g_fishtool_chown = {
	.name = "chown",
	.fn = fishtool_chown,
	.getopt_str = "g:U:",
	.usage = fishtool_chown_usage,
};
