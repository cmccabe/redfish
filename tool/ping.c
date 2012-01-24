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
	redfish_disconnect_and_free(cli);

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
