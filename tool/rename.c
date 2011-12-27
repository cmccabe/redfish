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

int fishtool_rename(struct fishtool_params *params)
{
	const char *src, *dst;
	int ret;
	struct redfish_client *cli = NULL;

	src = params->non_option_args[0];
	if (!src) {
		fprintf(stderr, "fishtool_chown: you must give a source path."
			"  -h for help.\n");
		ret = -EINVAL;
		goto done;
	}
	dst = params->non_option_args[0];
	if (!dst) {
		fprintf(stderr, "fishtool_chown: you must give a destination "
			"path.  -h for help.\n");
		ret = -EINVAL;
		goto done;
	}
	ret = redfish_connect(params->mlocs, params->user_name, &cli);
	if (ret) {
		fprintf(stderr, "redfish_connect failed with error %d\n", ret);
		goto done;
	}
	ret = redfish_rename(cli, src, dst);
	if (ret) {
		fprintf(stderr, "redfish_rename failed with error %d\n", ret);
		goto done;
	}
	ret = 0;
done:
	if (cli)
		redfish_disconnect(cli);
	return ret;
}

const char *fishtool_rename_usage[] = {
	"rename: move a file or directory.",
	"",
	"usage:",
	"rename <source-path> <destination-path>",
	NULL,
};

struct fishtool_act g_fishtool_rename = {
	.name = "rename",
	.fn = fishtool_rename,
	.getopt_str = "g:U:",
	.usage = fishtool_rename_usage,
};
