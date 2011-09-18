/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "jorm/json.h"
#include "mon/output_worker.h"
#include "mon/worker.h"
#include "util/error.h"
#include "util/macro.h"
#include "util/platform/pipe2.h"
#include "util/run_cmd.h"
#include "util/safe_io.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

struct output_worker_data
{
	int fd;
	int pid;
	int close_fd_on_exit;
};

struct worker *g_output_worker;

static int do_output(struct worker_msg *msg, void *data)
{
	int res, ret;
	struct json_object *joty;
	struct output_worker_data *odata = (struct output_worker_data*)data;
	struct output_worker_msg *omsg = (struct output_worker_msg*)msg;
	char len_buf[LBUF_LEN_DIGITS + 2] = { 0 };
	const char* jostr;
	size_t jostr_len;

	if (msg->ty != WORKER_MSG_OUTPUT_JSON) {
		ret = -EINVAL;
		goto done;
	}
	if (json_object_get_type(omsg->jo) != json_type_object) {
		ret = -EDOM;
		goto done_put_jo;
	}
	joty = json_object_new_int(omsg->json_ty);
	if (!joty) {
		ret = -ENOMEM;
		goto done_put_jo;
	}
	json_object_object_add(omsg->jo, "type", joty);
	jostr = json_object_get_string(omsg->jo);
	jostr_len = strlen(jostr);
	snprintf(len_buf, sizeof(len_buf),
		"\n\% " TO_STR2(LBUF_LEN_DIGITS) "Zd", jostr_len);
	res = safe_write(odata->fd, len_buf, LBUF_LEN_DIGITS + 1);
	res = safe_write(odata->fd, jostr, jostr_len);
	if (omsg->json_ty == MON_OUTPUT_MSG_END) {
		char newline[1] = "\n";
		res = safe_write(odata->fd, newline, 1);
	}
	ret = 0;
done_put_jo:
	json_object_put(omsg->jo);
done:
	return ret;
}

static void close_output_fd(void *data)
{
	int res;
	struct output_worker_data *odata = (struct output_worker_data*)data;
	if (odata->close_fd_on_exit) {
		RETRY_ON_EINTR(res, close(odata->fd));
	}
	odata->fd = -1;
	free(odata);
}

static int start_filter_give_input(const char *fishtop, int *pid)
{
	char fd_str[32] = { 0 };
	const char *cvec[] = { fishtop, "-f", fd_str, NULL };
	int mypid, res, ret, pipefd[2] = { -1, -1 };

	*pid = -1;
	/* Create a pipe to pass in data */
	ret = do_pipe2(pipefd, O_CLOEXEC);
	if (ret) {
		return FORCE_NEGATIVE(ret);
	}
	mypid = fork();
	if (mypid == -1) {
		RETRY_ON_EINTR(res, close(pipefd[PIPE_WRITE]));
		RETRY_ON_EINTR(res, close(pipefd[PIPE_READ]));
		return FORCE_NEGATIVE(errno);
	}
	else if (mypid == 0) {
		int curflags;
		RETRY_ON_EINTR(res, close(pipefd[PIPE_WRITE]));
		/* Don't close the read end of the pipe when we execute our
		 * subprocess */
		curflags = fcntl(pipefd[PIPE_READ], F_GETFL, 0);
		ret = fcntl(pipefd[PIPE_READ], F_SETFL, curflags & (~O_CLOEXEC));
		if (ret)
			_exit(127);
		snprintf(fd_str, sizeof(fd_str), "%d", pipefd[PIPE_READ]);
		execvp(cvec[0], (char**)cvec);
		_exit(127);
	}
	*pid = mypid;
	RETRY_ON_EINTR(res, close(pipefd[PIPE_READ]));
	pipefd[PIPE_READ] = -1;

	return pipefd[PIPE_WRITE];
}

static int output_worker_init_fishtop(const char *argv0,
		struct output_worker_data *odata)
{
	char fishtop[PATH_MAX];
	int ret;

	ret = get_colocated_path(argv0, "fishtop", fishtop, sizeof(fishtop));
	if (ret) {
		fprintf(stderr, "failed to find 'fishtop' : name too long\n");
		return ret;
	}
	if (access(fishtop, R_OK | X_OK)) {
		char fishtop2[PATH_MAX];
		ret = get_colocated_path(argv0, "../top/fishtop",
				fishtop2, sizeof(fishtop2));
		if (ret) {
			fprintf(stderr, "failed to find 'fishtop' : "
				"name too long\n");
			return ret;
		}
		if (access(fishtop2, R_OK | X_OK)) {
			fprintf(stderr, "failed to find '%s' or '%s'.\n",
				fishtop2, fishtop2);
			return ret;
		}
		strcpy(fishtop, fishtop2);
	}
	odata->close_fd_on_exit = 1;
	odata->fd = start_filter_give_input(fishtop, &odata->pid);
	if (odata->fd < 0) {
		ret = odata->fd;
		free(odata);
		fprintf(stderr, "start_filter_give_input failed with "
			"error %d\n", ret);
		return ret;
	}
	return 0;
}

int output_worker_init(const char *argv0, enum output_worker_sink_t sink)
{
	int ret;
	struct output_worker_data *odata =
		calloc(1, sizeof(struct output_worker_data));
	if (!odata)
		return -ENOMEM;

	switch (sink) {
	case MON_OUTPUT_SINK_NONE:
		odata->fd = open("/dev/null", O_WRONLY);
		if (odata->fd < 0) {
			ret = errno;
			fprintf(stderr, "failed to open /dev/null: error "
				"%d\n", ret);
			goto error;
		}
		odata->pid = -1;
		odata->close_fd_on_exit = 1;
		break;
	case MON_OUTPUT_SINK_STDOUT:
		odata->fd = STDOUT_FILENO;
		odata->pid = -1;
		odata->close_fd_on_exit = 0;
		break;
	case MON_OUTPUT_SINK_FISHTOP:
		ret = output_worker_init_fishtop(argv0, odata);
		if (ret)
			goto error;
		break;
	default:
		fprintf(stderr, "logic error on %s:%d\n", __FILE__, __LINE__);
		ret = -EDOM;
		goto error;
	}

	g_output_worker = worker_start("output_worker",
					do_output, close_output_fd, odata);
	if (!g_output_worker) {
		ret = -EDOM;
		goto error;
	}
	return 0;
error:
	close_output_fd(odata);
	return ret;
}

int output_worker_shutdown(void)
{
	int ret;
	ret = worker_stop(g_output_worker);
	if (ret)
		return ret;
	ret = worker_join(g_output_worker);
	if (ret)
		return ret;
	return 0;

}

int output_worker_sendmsg_or_free(struct worker *worker,
				  struct output_worker_msg *omsg)
{
	int ret = worker_sendmsg(worker, omsg);
	if (ret) {
		json_object_put(omsg->jo);
		free(omsg);
	}
	return ret;
}
