/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/glitch_log.h"
#include "jorm/json.h"
#include "mon/output_worker.h"
#include "mon/worker.h"
#include "util/error.h"
#include "util/macro.h"
#include "util/platform/pipe2.h"
#include "util/platform/socket.h"
#include "util/run_cmd.h"
#include "util/safe_io.h"
#include "util/string.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

static pthread_t g_output_worker_thread;

static int g_write_event_fd = -1;

#define MON_OUTPUT_WORKER_MAX_CONN 10

enum output_worker_conn_state
{
	OUTPUT_WORKER_CONN_DISC,
	OUTPUT_WORKER_CONN_NEW,
	OUTPUT_WORKER_CONN_ESTABLISHED,
};

struct output_worker_data
{
	char *sock_path;
	int sock;
	int event_fd[2];
	int conn[MON_OUTPUT_WORKER_MAX_CONN];
	enum output_worker_conn_state cstate[MON_OUTPUT_WORKER_MAX_CONN];
};

static void do_bind_and_listen(struct output_worker_data *odata,
			       char *err, size_t err_len)
{
	int ret;
	struct sockaddr_un address;
	odata->sock = do_socket(PF_UNIX, SOCK_STREAM, 0, WANT_O_CLOEXEC);
	if (odata->sock < 0) {
		snprintf(err, err_len,
			 "failed to create socket: error %d", odata->sock);
		goto error;
	}
	memset(&address, 0, sizeof(struct sockaddr_un));
	if (zsnprintf(address.sun_path, sizeof(address.sun_path),
				"%s", odata->sock_path)) {
		snprintf(err, err_len, "socket path too long!\n");
		goto error_close_fd;
	}
	address.sun_family = AF_UNIX;
	ret = 0;
	if (bind(odata->sock, (struct sockaddr*)&address,
			sizeof(struct sockaddr_un))) {
		ret = errno;
		if (ret == EADDRINUSE) {
			RETRY_ON_EINTR(ret, unlink(odata->sock_path));
			if (bind(odata->sock, (struct sockaddr*)&address,
					sizeof(struct sockaddr_un))) {
				ret = errno;
			}
		}
	}
	if (ret) {
		snprintf(err, err_len, "Failed to bind to the UNIX domain "
			 "socket '%s': error %d\n", odata->sock_path, ret);
		goto error_close_fd;
	}
	if (listen(odata->sock, 5)) {
		ret = errno;
		snprintf(err, err_len, "Failed to listen to the UNIX domain "
			 "socket '%s': error %d\n", odata->sock_path, ret);
		goto error_unlink_sock;
	}
	return;

error_unlink_sock:
	RETRY_ON_EINTR(ret, unlink(odata->sock_path));
error_close_fd:
	RETRY_ON_EINTR(ret, close(odata->sock));
error:
	return;
}

static void output_worker_accept_new_conn(struct output_worker_data *odata)
{
	int ret, i, cfd;
	struct sockaddr_un address;
	socklen_t alen = sizeof(struct sockaddr_un);
	RETRY_ON_EINTR(cfd, accept(odata->sock, (struct sockaddr*)&address, &alen));
	if (cfd < 0) {
		ret = errno;
		glitch_log("accept failed with error %d\n", ret);
		return;
	}
	for (i = 0; i < MON_OUTPUT_WORKER_MAX_CONN; ++i) {
		if (odata->cstate[i] == OUTPUT_WORKER_CONN_DISC) {
			odata->cstate[i] = OUTPUT_WORKER_CONN_NEW;
			odata->conn[i] = cfd;
			return;
		}
	}
	glitch_log("logic error on line %d of %s\n", __LINE__, __FILE__);
	RETRY_ON_EINTR(ret, close(cfd));
}

/*
static int write_json_msg(int fd, struct json_object *jo)
{
	jostr = json_object_get_string(omsg->jo);
	jostr_len = strlen(jostr);
	snprintf(len_buf, sizeof(len_buf),
		"\n\% " TO_STR2(LBUF_LEN_DIGITS) "Zd", jostr_len);
	res = safe_write(odata->fd, len_buf, LBUF_LEN_DIGITS + 1);
	if (res)
		return res;
	res = safe_write(odata->fd, jostr, jostr_len);
	if (res)
		return res;
	return 0;
}
*/

static void update_observers(struct output_worker_data *odata)
{
	int i, ret;

	for (i = 0; i < MON_OUTPUT_WORKER_MAX_CONN; ++i) {
		if (odata->cstate[i] == OUTPUT_WORKER_CONN_DISC)
			continue;
		else if (odata->cstate[i] == OUTPUT_WORKER_CONN_NEW) {
			char fu[] = "full_update";
			ret = safe_write(odata->conn[i], fu, strlen(fu));
			if (ret) {
				RETRY_ON_EINTR(ret, close(odata->conn[i]));
				odata->conn[i] = -1;
				odata->cstate[i] = OUTPUT_WORKER_CONN_DISC;
			}
			odata->cstate[i] = OUTPUT_WORKER_CONN_ESTABLISHED;
		}
		else if (odata->cstate[i] == OUTPUT_WORKER_CONN_ESTABLISHED) {
			char fu[] = "partial_update";
			ret = safe_write(odata->conn[i], fu, strlen(fu));
			if (ret) {
				RETRY_ON_EINTR(ret, close(odata->conn[i]));
				odata->conn[i] = -1;
				odata->cstate[i] = OUTPUT_WORKER_CONN_DISC;
			}
		}
	}
}

static void* run_output_worker(void *v)
{
	int ret, i;
	struct output_worker_data *odata = (struct output_worker_data*)v;

	while (1) {
		struct pollfd fds[2];
		memset(fds, 0, sizeof(fds));
		/* only listen for new connections if we have a free slot. */
		fds[0].fd = odata->event_fd[PIPE_READ];
		fds[0].events = 0;
		for (i = 0; i < MON_OUTPUT_WORKER_MAX_CONN; ++i) {
			if (odata->cstate[i] == OUTPUT_WORKER_CONN_DISC) {
				fds[0].events = POLLIN;
				break;
			}
		}
		fds[1].fd = odata->sock;
		fds[1].events = POLLIN | POLLRDBAND;
		ret = poll(fds, 2, -1);
		if (ret < 0) {
			ret = errno;
			if (ret == EINTR)
				continue;
			glitch_log("output_worker_data: poll error %d\n", ret);
			break;
		}
		if (fds[0].revents & POLLIN) {
			output_worker_accept_new_conn(odata);
		}
		if (fds[1].revents & POLLIN) {
			char buf[1] = { '\0' };
			ret = safe_read(odata->event_fd[PIPE_READ], buf, sizeof(buf));
			if (buf[0] == '\0') {
				/* shutdown. */
				break;
			}
			else if (buf[0] == '\1') {
				update_observers(odata);
			}
		}
	}

	for (i = 0; i < MON_OUTPUT_WORKER_MAX_CONN; ++i) {
		if (odata->cstate[i] != OUTPUT_WORKER_CONN_DISC) {
			RETRY_ON_EINTR(ret, close(odata->conn[i]));
		}
		odata->cstate[i] = OUTPUT_WORKER_CONN_DISC;
	}
	RETRY_ON_EINTR(ret, close(odata->sock));
	RETRY_ON_EINTR(ret, close(odata->event_fd[PIPE_READ]));
	g_write_event_fd = -1;
	free(odata->sock_path);
	free(odata);
	return NULL;
}

void init_output_worker(const char* sock_path, char *err, size_t err_len)
{
	struct output_worker_data *odata;
	pthread_attr_t attr;
	int ret, i;

	odata = calloc(1, sizeof(struct output_worker_data));
	if (!odata) {
		snprintf(err, err_len, "out of memory\n");
		goto error;
	}
	odata->sock_path = strdup(sock_path);
	if (!odata->sock_path) {
		snprintf(err, err_len, "out of memory\n");
		goto error_free_odata;
	}
	ret = do_pipe2(odata->event_fd, O_CLOEXEC);
	if (ret) {
		snprintf(err, err_len, "failed to open pipe: error %d\n", ret);
		goto error_free_sock_path;
	}
	do_bind_and_listen(odata, err, err_len);
	if (err[0])
		goto error_close_pipe;
	for (i = 0; i < MON_OUTPUT_WORKER_MAX_CONN; ++i) {
		odata->cstate[i] = OUTPUT_WORKER_CONN_DISC;
	}
	if (pthread_attr_init(&attr) || (pthread_attr_setdetachstate(
			&attr, PTHREAD_CREATE_DETACHED)) ||
			pthread_create(&g_output_worker_thread, NULL,
				run_output_worker, (void*)odata)) {
		snprintf(err, err_len, "failed to create pthread. "
			 "error %d\n", ret);
		goto error_close_sock;
	}
	g_write_event_fd = odata->event_fd[PIPE_WRITE];
	return;

error_close_sock:
	RETRY_ON_EINTR(ret, close(odata->sock));
error_close_pipe:
	RETRY_ON_EINTR(ret, close(odata->event_fd[PIPE_READ]));
	RETRY_ON_EINTR(ret, close(odata->event_fd[PIPE_WRITE]));
error_free_sock_path:
	free(odata->sock_path);
error_free_odata:
	free(odata);
error:
	return;
}

static void kick_output_worker_impl(char m)
{
	int res;
	char buf[1] = { m };
	res = safe_write(g_write_event_fd, buf, 1);
}

void kick_output_worker(void)
{
	kick_output_worker_impl('\1');
}

void shutdown_output_worker(void)
{
	kick_output_worker_impl('\0');
}
