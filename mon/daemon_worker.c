/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/glitch_log.h"
#include "jorm/jorm_const.h"
#include "mon/daemon_worker.h"
#include "mon/mon_config.h"
#include "mon/ssh.h"
#include "mon/worker.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/run_cmd.h"
#include "util/simple_io.h"
#include "util/string.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

struct worker **g_daemon_workers;

static int handle_dw_ssh(struct worker_msg_ssh *m, struct mon_daemon *md)
{
	char out[512] = { 0 };
	return ssh_exec(md->host, out, sizeof(out), m->args);
}

static const char *type_to_binary_name(const char *ty)
{
	if (strcmp(ty, "mds"))
		return "fishmds";
	else if (strcmp(ty, "osd"))
		return "fishosd";
	else
		return NULL;
}

static int handle_dw_ssh_up_bin(const struct worker_msg_upload_bin *m,
				struct mon_daemon *md)
{
	int ret, ifd, ofd, pid;
	const char *fname;
	char ibin_path[PATH_MAX], obin_path[PATH_MAX];
	const char *cmd[] = { "cat", "-", ">", obin_path, NULL };

	if (m->bin[0] == '\0') {
		fname = type_to_binary_name(md->type);
		if (!fname) {
			glitch_log("handle_dw_ssh_up_bin: should have complained "
				   "about bad md->type before this. md->type = '%s'\n",
				   md->type);
			ret = -EDOM;
			goto done;
		}
	}
	else {
		fname = m->bin[0];
	}
	/* src_bindir can't be NULL here because it defaults to the fishmon
	 * directory */
	if (zsnprintf(ibin_path, PATH_MAX, "%s/%s", md->src_bindir, fname)) {
		glitch_log("handle_dw_ssh_up_bin: md->src_bindir path '%s' "
			   "is too long!\n", md->src_bindir);
		ret = -EDOM;
		goto done;
	}
	if (md->dst_bindir == JORM_INVAL_STR) {
		ret = shell_escape(ibin_path, obin_path, sizeof(obin_path));
		if (ret) {
			glitch_log("handle_dw_ssh_up_bin: output bin path is "
				   "too long!");
			ret = -EDOM;
			goto done;
		}
	}
	else {
		char tmp[PATH_MAX];
		if (zsnprintf(tmp, PATH_MAX, "%s/%s", md->dst_bindir, fname)) {
			glitch_log("handle_dw_ssh_up_bin: md->dst_bindir "
				"path '%s' is too long!\n", md->src_bindir);
			ret = -EDOM;
			goto done;
		}
		ret = shell_escape(tmp, obin_path, sizeof(obin_path));
		if (ret) {
			glitch_log("handle_dw_ssh_up_bin: md->dst_bindir "
				   "path is too long!\n");
			ret = -EDOM;
			goto done;
		}
	}
	ifd = open(ibin_path, O_RDONLY);
	if (ifd < 0) {
		ret = errno;
		glitch_log("handle_dw_ssh_up_bin: failed to open '%s': "
			   "error %d\n", ibin_path, ret);
		goto done;
	}
	ofd = start_ssh_give_input(md->host, cmd, &pid);
	if (ofd < 0) {
		int res;
		glitch_log("handle_dw_ssh_up_bin: start_ssh_give_input "
			   "failed with error '%d'\n", ofd);
		RETRY_ON_EINTR(res, close(ifd));
		ret = ofd;
		goto done;
	}
	ret = copy_fd_to_fd(ifd, ofd);
	if (ret) {
		if (ret & COPY_FD_TO_FD_SRCERR) {
			ret &= (~COPY_FD_TO_FD_SRCERR);
			glitch_log("handle_dw_ssh_up_bin: error reading '%s': "
				   "error '%d'\n", ibin_path, ret);
			ret = -EIO;
			goto done;
		}
		else {
			glitch_log("handle_dw_ssh_up_bin: copy_fd_to_fd "
				   "failed with error '%d'\n", ret);
			ret = -REDFISH_TEMP_ERROR;
			goto done;
		}
	}
	if (ifd > 0) {
		RETRY_ON_EINTR(ret, close(ifd));
		ifd = -1;
	}
	if (ofd > 0) {
		RETRY_ON_EINTR(ret, close(ofd));
		ofd = -1;
		if (ret) {
			glitch_log("handle_dw_ssh_up_bin: error writing '%s': "
				   "error '%d'\n", ibin_path, ret);
			ret = -REDFISH_TEMP_ERROR;
			goto done;
		}
	}
done:
	if (ifd > 0) {
		RETRY_ON_EINTR(ret, close(ifd));
		ifd = -1;
	}
	if (ofd > 0) {
		RETRY_ON_EINTR(ret, close(ofd));
		ofd = -1;
	}
	ret = do_waitpid(pid);
	if (ret != 0) {
		glitch_log("handle_dw_ssh_up_bin: do_waitpid "
			   "failed with error '%d'\n", ret);
		return -REDFISH_TEMP_ERROR;
	}
	return 0;
}

static int handle_daemon_worker_msg(struct worker_msg *m, void *data)
{
	struct mon_daemon* md = (struct mon_daemon*)data;
	switch (m->ty) {
	case WORKER_MSG_SSH:
		return handle_dw_ssh((struct worker_msg_ssh*)m, md);
	case WORKER_MSG_SSH_UP_BIN:
		return handle_dw_ssh_up_bin(
			(struct worker_msg_upload_bin*)m, md);
	default:
		return -EINVAL;
	}
	return 0;
}

static void free_mon_daemon(void *data)
{
	struct mon_daemon* md = (struct mon_daemon*)data;
	JORM_FREE_mon_daemon(md);
}

void init_daemon_workers(const struct mon_cluster *cluster,
			 char *err, size_t err_len)
{
	struct mon_daemon *md = NULL;
	int idx = 0, num_d;
	for (num_d = 0; cluster->daemons[num_d]; ++num_d) {
		;
	}
	g_daemon_workers = calloc(num_d + 1, sizeof(struct worker*));
	if (!g_daemon_workers)
		goto oom_error;
	for (idx = 0; idx < num_d; ++idx) {
		char name[WORKER_NAME_MAX];
		snprintf(name, WORKER_NAME_MAX, "daemon%04d", idx + 1);
		md = JORM_INIT_mon_daemon();
		if (!md)
			goto oom_error;
		if (JORM_COPY_mon_daemon(cluster->defaults, md))
			goto oom_error;
		if (JORM_COPY_mon_daemon(cluster->daemons[idx], md))
			goto oom_error;
		md->idx = idx;
		g_daemon_workers[idx] = worker_start(name,
			handle_daemon_worker_msg, free_mon_daemon, md);
		if (!g_daemon_workers[idx]) {
			snprintf(err, err_len, "failed to create %s\n", name);
			goto error;
		}
		md = NULL;
	}
	return;
oom_error:
	if (md)
		JORM_FREE_mon_daemon(md);
	snprintf(err, err_len, "out of memory");
error:
	for (--idx; idx > 0; --idx) {
		worker_stop(g_daemon_workers[idx]);
		worker_join(g_daemon_workers[idx]);
	}
	return;
}

void shutdown_daemon_workers(void)
{
	int num_di;
	for (num_di = 0; g_daemon_workers[num_di]; ++num_di) {
		worker_stop(g_daemon_workers[num_di]);
		worker_join(g_daemon_workers[num_di]);
	}
}

int daemon_worker_ssh(struct worker *w, sem_t *sem, ...)
{
	int ret, num_args = 0;
	char *c;
	va_list ap;
	struct worker_msg_ssh *nm, *m =
		calloc(1, sizeof(struct worker_msg_ssh) + sizeof(char*));
	if (!m) {
		ret = -ENOMEM;
		goto error;
	}

	va_start(ap, sem);
	while (1) {
		c = va_arg(ap, char*);
		if (c == NULL)
			break;
		++num_args;
		nm = realloc(m, sizeof(struct worker_msg_ssh) +
			((num_args + 1) * sizeof(char*)));
		if (!nm) {
			ret = -ENOMEM;
			goto error;
		}
		m = nm;
		m->args[num_args - 1] = c;
		m->args[num_args] = NULL;
	}
	m->m.ty = WORKER_MSG_SSH;
	m->sem = sem;
	va_end(ap);
	ret = worker_sendmsg(w, m);
	if (ret == 0)
		return ret;
error:
	free(m);
	return ret;
}

int daemon_worker_upload_binary(struct worker *w, sem_t *sem, const char *bin)
{
	int txt_len;
	struct worker_msg_upload_bin *m;

	txt_len = (bin) ? strlen(bin) + 1: 1;
	m = calloc(1, sizeof(struct worker_msg_upload_bin) + txt_len);
	if (!m)
		return -ENOMEM;

	m->m.ty = WORKER_MSG_SSH_UP_BIN;
	m->sem = sem;
	if (bin)
		strcpy(m->bin[0], bin);
	else
		m->bin[0] = '\0';
	return worker_sendmsg_or_free(w, m);
}
