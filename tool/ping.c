/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "client/fishc.h"
#include "tool/main.h"

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

int fishtool_ping(struct fishtool_params *params)
{
	int ret;
	struct redfish_client *cli;

	ret = redfish_connect(params->mlocs, params->user_name, &cli);
	if (ret) {
		fprintf(stderr, "redfish_connect failed with error %d\n", ret);
		return ret;
	}
	redfish_disconnect(cli);

	return 0;
}

const char *fishtool_ping_usage[] = {
	"ping: connect to the metadata servers and then immediately close ",
	"the connection",
	NULL,
};

struct fishtool_act g_fishtool_ping = {
	.name = "ping",
	.fn = fishtool_ping,
	.getopt_str = "",
	.usage = fishtool_ping_usage,
};
