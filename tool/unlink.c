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

int fishtool_unlink(struct fishtool_params *params)
{
	const char *path;
	int ret, recursive;
	struct redfish_client *cli = NULL;

	recursive = 0; //!!params->lowercase_args[ALPHA_IDX('r')];
	path = params->non_option_args[0];
	if (!path) {
		fprintf(stderr, "fishtool_unlink: you must give a path name "
			"to unlink. -h for help.\n");
		ret = -EINVAL;
		goto done;
	}
	ret = redfish_connect(params->mlocs, params->user_name, &cli);
	if (ret) {
		fprintf(stderr, "redfish_connect failed with error %d\n", ret);
		goto done;
	}
	if (recursive) {
		ret = redfish_unlink_tree(cli, path);
		if (ret) {
			fprintf(stderr, "redfish_unlink_tree failed with "
				"error %d\n", ret);
			goto done;
		}
	}
	else {
		ret = redfish_unlink(cli, path);
		if (ret) {
			fprintf(stderr, "redfish_unlink failed with error "
				"%d\n", ret);
			goto done;
		}
	}
	ret = 0;
done:
	if (cli)
		redfish_disconnect(cli);
	return ret;
}

const char *fishtool_unlink_usage[] = {
	"unlink: change the permissions of a file or directory.",
	"",
	"usage:",
	"unlink [options] <path-name>",
	"",
	"options:",
	"-r             unlink recusively",
	NULL,
};

struct fishtool_act g_fishtool_unlink = {
	.name = "unlink",
	.fn = fishtool_unlink,
	.getopt_str = "r",
	.usage = fishtool_unlink_usage,
};
