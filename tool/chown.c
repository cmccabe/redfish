/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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
			"to create. -h for help.\n");
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
		redfish_disconnect(cli);
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
