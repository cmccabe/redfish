/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "mon/ssh.h"
#include "util/error.h"
#include "util/compiler.h"
#include "util/run_cmd.h"
#include "util/test.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CVEC 50

static int append_arg(const char ** cvec, int *idx, const char* arg)
{
	if (*idx == MAX_CVEC)
		return -ENOBUFS;
	cvec[*idx] = arg;
	*idx = *idx + 1;
	return 0;
}

static int ssh_append_args(const char **cmd, const char *host,
		const char *canary, const char **cvec, int *idx)
{
	int ret;

	append_arg(cvec, idx, "ssh");
	append_arg(cvec, idx, "-o");
	append_arg(cvec, idx, "PasswordAuthentication=no");
	append_arg(cvec, idx, "-x");
	append_arg(cvec, idx, host);
	if (canary) {
		append_arg(cvec, idx, "echo");
		append_arg(cvec, idx, canary);
		append_arg(cvec, idx, "&&");
	}
	for (; *cmd; ++cmd) {
		ret = append_arg(cvec, idx, *cmd);
		if (ret)
			return ret;
	}
	ret = append_arg(cvec, idx, NULL);
	if (ret)
		return ret;
	return 0;
}

int ssh_exec(const char *host, char *out, size_t out_len, const char **cmd)
{
	int idx = 0, ret;
	const char* cvec[MAX_CVEC];
	const char canary[] = "ONEFISH_CONNECTED";
	const char canary_plus_newline[] = "ONEFISH_CONNECTED\n";

	if (out_len < sizeof(canary_plus_newline))
		return -EDOM;
	ret = ssh_append_args(cmd, host, canary, cvec, &idx);
	if (ret)
		return FORCE_NEGATIVE(ret);
	ret = run_cmd_get_output(out, out_len, cvec);
	if (ret == 0)
		return 0;
	if (memcmp(out, canary_plus_newline, sizeof(canary_plus_newline))) {
		return FORCE_NEGATIVE(ONEFISH_SSH_ERR);
	}
	memmove(out, out + sizeof(canary_plus_newline),
		out_len - sizeof(canary_plus_newline));
	memset(out + out_len - sizeof(canary_plus_newline), 0,
		sizeof(canary_plus_newline));
	return ret;
}

int start_ssh_give_input(const char *host, const char **cmd, int *pid)
{
	int idx = 0, ret;
	const char* cvec[MAX_CVEC];

	ret = ssh_append_args(cmd, host, NULL, cvec, &idx);
	if (ret)
		return FORCE_NEGATIVE(ret);
	return start_cmd_give_input(cvec, pid);
}
