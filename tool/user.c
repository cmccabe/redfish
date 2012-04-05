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
#include "util/terror.h"

#include <ctype.h>
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

#define FISHTOOL_USERMOD_LIST_BUF_SZ 16384

typedef int (*fishtool_usermod_fn_t)(struct fishtool_params *,
		struct redfish_client *, const char *, const char *);

static int fishtool_usermod_add(
	POSSIBLY_UNUSED(struct fishtool_params *params),
	struct redfish_client *cli, const char *tgt_user, const char *group)
{
	int ret;

	ret = redfish_add_user_to_group(cli, tgt_user, group);
	if (ret) {
		fprintf(stderr, "error adding user '%s' to group '%s': error "
			"code %d (%s)\n", tgt_user, group, ret, terror(ret));
	}
	return ret;
}

static int fishtool_usermod_set_primary(
	POSSIBLY_UNUSED(struct fishtool_params *params),
	struct redfish_client *cli, const char *tgt_user, const char *group)
{
	int ret;

	ret = redfish_set_primary_user_group(cli, tgt_user, group);
	if (ret) {
		fprintf(stderr, "error setting primary group of  user '%s' "
			"to '%s': error code %d (%s)\n",
			tgt_user, group, ret, terror(ret));
	}
	return ret;
}

static int fishtool_usermod_list(
	POSSIBLY_UNUSED(struct fishtool_params *params),
	struct redfish_client *cli, const char *tgt_user,
	POSSIBLY_UNUSED(const char *group))
{
	char *buf;
	int ret;

	buf = calloc(1, FISHTOOL_USERMOD_LIST_BUF_SZ);
	if (!buf) {
		fprintf(stderr, "out of memory\n");
		return -ENOMEM;
	}
	ret = redfish_get_user_info(cli, tgt_user, buf,
		FISHTOOL_USERMOD_LIST_BUF_SZ - 1);
	if (ret) {
		fprintf(stderr, "error removing user '%s' from group "
			"'%s': error code %d (%s)\n",
			tgt_user, group, ret, terror(ret));
		free(buf);
		return ret;
	}
	puts(buf);
	free(buf);
	return 0;
}

static int fishtool_usermod_remove(
	POSSIBLY_UNUSED(struct fishtool_params *params),
	struct redfish_client *cli, const char *tgt_user, const char *group)
{
	int ret;

	ret = redfish_remove_user_from_group(cli, tgt_user, group);
	if (ret) {
		fprintf(stderr, "error removing user '%s' from group "
			"'%s': error code %d (%s)\n",
			tgt_user, group, ret, terror(ret));
	}
	return ret;
}

static int usermod_checkparam(struct fishtool_params *params, char opt,
		const char **group, fishtool_usermod_fn_t *fn,
		fishtool_usermod_fn_t tgt_fn)
{
	const char *str;

	if (islower(opt))
		str = params->lowercase_args[ALPHA_IDX(opt)];
	else
		str = params->uppercase_args[ALPHA_IDX(opt)];
	if (!str)
		return 0;
	if (*fn) {
		fprintf(stderr, "fishtool_usermod: you cannot specify more "
			"than one operation at once.  -h for help.\n");
		return -EINVAL;
	}
	*group = str;
	*fn = tgt_fn;
	return 0;
}

int fishtool_usermod(struct fishtool_params *params)
{
	int ret;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	const char *group = NULL;
	const char *tgt_user;
	fishtool_usermod_fn_t fn = NULL;
	struct redfish_client *cli = NULL;

	ret = usermod_checkparam(params, 'a', &group, &fn,
			fishtool_usermod_add);
	if (ret)
		return ret;
	ret = usermod_checkparam(params, 'g', &group, &fn,
			fishtool_usermod_set_primary);
	if (ret)
		return ret;
	ret = usermod_checkparam(params, 'l', &group, &fn,
			fishtool_usermod_list);
	if (ret)
		return ret;
	ret = usermod_checkparam(params, 'r', &group, &fn,
			fishtool_usermod_remove);
	if (ret)
		return ret;
	if (!fn) {
		fprintf(stderr, "fishtool_usermod: you must specify an "
			"action\n");
		return -EINVAL;
	}
	if (params->uppercase_args[ALPHA_IDX('U')])
		tgt_user = params->uppercase_args[ALPHA_IDX('U')];
	else
		tgt_user = params->user_name;
	cli = redfish_connect(params->cpath, params->user_name,
		redfish_log_to_stderr, NULL, err, err_len);
	if (err[0]) {
		fprintf(stderr, "redfish_connect: failed to connect: "
				"%s\n", err);
		return -EIO;
	}
	ret = fn(params, cli, tgt_user, group);
	redfish_disconnect_and_release(cli);
	return ret;
}

static const char *fishtool_usermod_usage[] = {
	"usermod: modify Redfish user properties",
	"",
	"usage:",
	"usermod [options]",
	"",
	"options:",
	"-a <group-name>        Add the user to <group-name>",
	"-g <group-name>        Set the user's primary group",
	"-l                     List information about the user",
	"-r <group-name>        Remove the user from a group",
	"-U <target-user-name>  User to operate on (default: self)",
	"                       Only superusers can modify users other than "
	"                       themselves.",
	NULL,
};

struct fishtool_act g_fishtool_usermod = {
	.name = "usermod",
	.fn = fishtool_usermod,
	.getopt_str = "a:g:lr:U:",
	.usage = fishtool_usermod_usage,
};
