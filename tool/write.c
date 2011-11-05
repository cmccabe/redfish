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

int fishtool_write(struct fishtool_params *params)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	const char *path, *local, *mode_str;
	int fd, res, ret, mode = 0644;
	struct redfish_client *cli = NULL;
	struct redfish_file *ofe = NULL;

	path = params->non_option_args[0];
	if (!path) {
		fprintf(stderr, "fishtool_write: you must give a path name "
			"to create. -h for help.\n");
		ret = -EINVAL;
		goto done;
	}
	local = params->lowercase_args[LALPHA_IDX('i')];
	mode_str = params->lowercase_args[LALPHA_IDX('p')];
	if (mode_str) {
		str_to_int(mode_str, 8, &mode, err, err_len);
		if (err[0]) {
			fprintf(stderr, "fishtool_write: error parsing -p: "
				"%s\n", err);
			ret = -EINVAL;
			goto done;
		}
	}
	ret = redfish_connect(params->mlocs, params->user_name, &cli);
	if (ret) {
		fprintf(stderr, "redfish_connect failed with error %d\n", ret);
		goto done;
	}
	ret = redfish_create(cli, path, mode, 0, 0, 0, &ofe);
	if (ret) {
		fprintf(stderr, "redfish_create failed with error %d\n", ret);
		goto done;
	}
	if (local) {
		fd = open(local, O_RDONLY);
		if (fd < 0) {
			ret = -errno;
			fprintf(stderr, "fishtool_write: error opening "
				"%s: %d\n", local, ret);
			goto done;
		}
	}
	else {
		fd = STDIN_FILENO;
	}
	while (1) {
		int len;
		char buf[8192];

		len = safe_read(fd, buf, sizeof(buf));
		if (len < 0) {
			ret = len;
			fprintf(stderr, "fishtool_write: error reading "
				"%s: %d\n", local, ret);
			goto done;
		}
		if (len == 0) {
			break;
		}
		ret = redfish_write(ofe, buf, len);
		if (ret) {
			fprintf(stderr, "redfish_write: error writing "
				"%d bytes to %s: %d\n", len, path, ret);
			goto done;
		}
	}
	ret = redfish_close(ofe);
	ofe = NULL;
	if (ret) {
		fprintf(stderr, "redfish_close failed with error %d\n", ret);
		goto done;
	}
	ret = 0;
done:
	if ((fd != STDIN_FILENO) && (fd > 0)) {
		RETRY_ON_EINTR(res, close(fd));
	}
	if (ofe)
		redfish_close(ofe);
	if (cli)
		redfish_disconnect(cli);
	return ret;
}

const char *fishtool_write_usage[] = {
	"write: create a new file and write some data to it.",
	"",
	"usage:",
	"write [options] <file-name>",
	"",
	"options:",
	"-i             local file to upload",
	"               If no local file is given, stdin will be used.",
	"-p             permissions (mode bits) to use",
	NULL,
};

struct fishtool_act g_fishtool_write = {
	.name = "write",
	.fn = fishtool_write,
	.getopt_str = "i:p:",
	.usage = fishtool_write_usage,
};
