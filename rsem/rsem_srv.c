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

#include "core/glitch_log.h"
#include "rsem/rsem_srv.h"
#include "rsem/rsem.h"
#include "util/macro.h"
#include "util/error.h"
#include "util/net.h"
#include "util/platform/pipe2.h"
#include "util/platform/socket.h"
#include "util/safe_io.h"
#include "util/string.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <json/json.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct rsem
{
	/** semaphore name */
	char *name;

	/** Current semaphore value */
	int val;

	/** FIFO queue of remote waiters. */
	struct rsem_waiter **wait;
};

struct rsem_server
{
	/** The semaphore server thread */
	pthread_t thread;

	/** Socket for accepting inbound connections */
	int sock;

	/** Pipe used to shut down the semaphore server */
	int event_fd[2];

	/** semaphore hash table */
	struct rsem *rls;

	/** Total size of semaphore hash table */
	int nrls;
};

/** Locate a remote semaphore in the remote semaphore hash table. */
struct rsem *get_rsem(struct rsem *rls, int nrls, const char *name)
{
	uint32_t h = ohash_str(name);
	int idx, seen = 0;

	idx = h % nrls;
	while (1) {
		struct rsem *rl = &rls[idx];
		if (strcmp(rl->name, name) == 0)
			return rl;
		idx = (idx + 1) % nrls;
		if (++seen == nrls)
			return NULL;
	}
}

/** Add a remote semaphore to the remote semaphore hash table. */
struct rsem *add_rsem(struct rsem *rls, int nrls, const char *name)
{
	uint32_t h = ohash_str(name);
	int idx, seen = 0;

	idx = h % nrls;
	while (1) {
		struct rsem *rl = &rls[idx];
		if (rl->name == NULL) {
			rl->name = strdup(name);
			if (!rl->name)
				return NULL;
			return rl;
		}
		idx = (idx + 1) % nrls;
		if (++seen == nrls)
			return NULL;
	}
}

static int handle_take_rsem(struct rsem_server *rss, int fd,
		struct rsem_request *lreq, unsigned long s_addr)
{
	int ret;
	struct rsem *rl;
	struct rsem_waiter *w;

	rl = get_rsem(rss->rls, rss->nrls, lreq->name);
	if (!rl) {
		return write_u32("handle_take_rsem", fd,
				 RSEM_SERVER_NO_SUCH_SEM);
	}
	if (rl->val > 0) {
		ret = write_u32("handle_take_rsem", fd, RSEM_SERVER_GIVE_SEM);
		if (ret)
			return ret;
		rl->val--;
		return 0;
	}
	if (lreq->port == 0) {
		/* This was a no-delay request */
		return write_u32("handle_take_rsem", fd, RSEM_SERVER_NACK);
	}
	w = JORM_OARRAY_APPEND_rsem_waiter(&rl->wait);
	if (!w) {
		glitch_log("handle_take_rsem: out of memory\n");
		return write_u32("handle_take_rsem", fd,
				 RSEM_SERVER_INTERNAL_ERROR);
	}
	w->port = lreq->port;
	w->s_addr = s_addr;
	ret = write_u32("handle_take_rsem", fd, RSEM_SERVER_DELAY_SEM);
	if (ret) {
		JORM_OARRAY_REMOVE_rsem_waiter(&rl->wait, w);
		return ret;
	}
	return 0;
}

static void free_rsems(struct rsem* rls, int nrls)
{
	int i;
	for (i = 0; i < nrls; ++i) {
		free(rls[i].name);
		JORM_OARRAY_FREE_rsem_waiter(&rls[i].wait);
	}
	free(rls);
}

static int init_rsems(struct rsem_server *rss, struct rsem_server_conf *conf)
{
	struct rsem *rls;
	struct rsem_conf **lc;
	int nrls;

	for (nrls = 0, lc = conf->sems; *lc; ++lc) {
		++nrls;
	}
	nrls *= 2;
	rls = calloc(nrls, sizeof(struct rsem));
	if (!rls) {
		return -ENOMEM;
	}

	for (lc = conf->sems; *lc; ++lc) {
		struct rsem *rl;
		rl = add_rsem(rls, nrls, (*lc)->name);
		if (!rl) {
			free_rsems(rl, nrls);
			return -ENOMEM;
		}
		rl->val = (*lc)->init_val;
	}
	rss->rls = rls;
	rss->nrls = nrls;
	return 0;
}

static int wake_waiter(struct rsem *rl, struct rsem_waiter *w)
{
	int ret, zfd = -1;
	struct json_object *jo;
	struct rsem_grant grant;
	struct sockaddr_in addr;
	uint32_t resp;

	memset(&grant, 0, sizeof(grant));
	grant.name = rl->name;
	jo = JORM_TOJSON_rsem_grant(&grant);
	if (!jo) {
		ret = -ENOMEM;
		goto done;
	}
	zfd = do_socket(AF_INET, SOCK_STREAM, 0, WANT_O_CLOEXEC);
	if (zfd < 0) {
		ret = zfd;
		glitch_log("wake_waiter: socket error: %d\n", ret);
		goto done;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = w->s_addr;
	addr.sin_port = htons(w->port);
	if (connect(zfd, &addr, sizeof(addr)) < 0) {
		const char *b;
		char obuf[INET_ADDRSTRLEN + 1] = { 0 };

		ret = -errno;
		b = inet_ntop(AF_INET, &addr, obuf, sizeof(obuf));
		glitch_log("wake_waiter: failed to conenct to %s: "
			   "error %d\n", b, ret);
		goto done;
	}
	if (write_u32("wake_waiter", zfd, RSEM_SERVER_GIVE_SEM)) {
		ret = -EIO;
		goto done;
	}
	if (blocking_write_json_req("wake_waiter", zfd, jo)) {
		ret = -EIO;
		goto done;
	}
	if (safe_read(zfd, &resp, sizeof(uint32_t)) != sizeof(uint32_t)) {
		glitch_log("wake_waiter: short read of response\n");
		ret = -EIO;
		goto done;
	}
	resp = ntohl(resp);
	if (resp == RSEM_CLIENT_ACK)
		ret = 0;
	else
		ret = -EINVAL;

done:
	if (jo)
		json_object_put(jo);
	if (zfd >= 0)
		RETRY_ON_EINTR(ret, close(zfd));
	return ret;
}

static void wake_any_waiter(struct rsem *rl)
{
	struct rsem_waiter **w;

	if ((!rl->wait) || (!rl->wait[0])) {
		/* There are no waiters. */
		return;
	}
	for (w = rl->wait; *w; ++w) {
		if (wake_waiter(rl, *w) == 0) {
			JORM_OARRAY_REMOVE_rsem_waiter(&rl->wait, *w);
			return;
		}
	}
	/* No waiters responded, so return without doing anything. */
}

static void rsem_server_handle_conn(struct rsem_server *rss)
{
	int res, fd = -1;
	struct sockaddr_in cli_addr;
	uint32_t ty;
	struct json_object *jo;
	struct rsem_request *lreq = NULL;
	struct rsem_release *lrel = NULL;
	socklen_t cli_len;

	cli_len = sizeof(cli_addr);
	fd = accept(rss->sock, (struct sockaddr *)&cli_addr, &cli_len);
	if (fd < 0) {
		int ret = -errno;
		glitch_log("accept error: %d\n", ret);
		goto done;
	}
	if (safe_read(fd, &ty, sizeof(uint32_t)) != sizeof(uint32_t)) {
		glitch_log("rsem_server_handle_conn: short read of type\n");
		goto done;
	}
	ty = ntohl(ty);
	switch (ty) {
	case RSEM_CLIENT_REQ_SEM: {
		if (blocking_read_json_req("rsem_server_handle_conn", fd, &jo))
			goto done;
		lreq = JORM_FROMJSON_rsem_request(jo);
		handle_take_rsem(rss, fd, lreq, cli_addr.sin_addr.s_addr);
		break;
	}
	case RSEM_CLIENT_REL_SEM: {
		struct rsem *rl;

		if (blocking_read_json_req("rsem_server_handle_conn", fd, &jo))
			goto done;
		lrel = JORM_FROMJSON_rsem_release(jo);
		rl = get_rsem(rss->rls, rss->nrls, lrel->name);
		if (rl)
			ty = RSEM_SERVER_ACK;
		else
			ty = RSEM_SERVER_NO_SUCH_SEM;
		if (write_u32("rsem_server_handle_conn", fd, ty)) {
			goto done;
		}
		rl->val++;
		if (rl) {
			wake_any_waiter(rl);
		}
		break;
	}
	default:
		glitch_log("rsem_server_handle_conn: received unknown "
			   "message type %d\n", ty);
		goto done;
	}
done:
	if (lreq)
		JORM_FREE_rsem_request(lreq);
	if (lrel)
		JORM_FREE_rsem_release(lrel);
	if (jo)
		json_object_put(jo);
	if (fd >= 0)
		RETRY_ON_EINTR(res, close(fd));
}

static void* rsem_server_loop(void *v)
{
	struct rsem_server *rss = (struct rsem_server*)v;

	while (1) {
		int ret;
		struct pollfd fds[2];

		memset(fds, 0, sizeof(fds));
		fds[0].fd = rss->sock;
		fds[0].events = POLLIN | POLLRDBAND;
		fds[1].fd = rss->event_fd[PIPE_READ];
		fds[1].events = POLLIN | POLLRDBAND;
		ret = poll(fds, 2, -1);
		if (ret < 0) {
			ret = errno;
			if (ret == EINTR)
				continue;
			glitch_log("rsem_server_loop: poll error %d\n", ret);
			break;
		}
		if (fds[0].revents & POLLIN) {
			rsem_server_handle_conn(rss);
		}
		if (fds[1].revents & POLLIN) {
			char buf[1] = { '\0' };
			ret = safe_read(rss->event_fd[PIPE_READ], buf,
					sizeof(buf));
			if (buf[0] == '\0') {
				/* shutdown. */
				break;
			}
		}
	}
	return NULL;
}

static void rsem_server_free(struct rsem_server *rss)
{
	int res;
	if (rss->sock >= 0)
		RETRY_ON_EINTR(res, close(rss->sock));
	if (rss->event_fd[PIPE_WRITE] != -1)
		RETRY_ON_EINTR(res, close(rss->event_fd[PIPE_WRITE]));
	if (rss->event_fd[PIPE_READ] != -1)
		RETRY_ON_EINTR(res, close(rss->event_fd[PIPE_READ]));
	if (rss->rls)
		free_rsems(rss->rls, rss->nrls);
	free(rss);
}

struct rsem_server* start_rsem_server(struct rsem_server_conf *conf,
					char *err, size_t err_len)
{
	int ret;
	struct rsem_server *rss = calloc(1, sizeof(struct rsem_server));
	if (!rss) {
		snprintf(err, err_len, "out of memory");
		goto error;
	}
	rss->sock = -1;
	rss->event_fd[PIPE_WRITE] = -1;
	rss->event_fd[PIPE_READ] = -1;
	ret = do_pipe2(rss->event_fd, WANT_O_CLOEXEC);
	if (ret) {
		snprintf(err, err_len, "failed to open pipe: error %d\n", ret);
		goto error;
	}
	rss->sock = do_bind_and_listen(conf->port, err, err_len);
	if (err[0]) {
		goto error;
	}
	ret = init_rsems(rss, conf);
	if (ret) {
		snprintf(err, err_len, "error initializing rsems: error %d",
			 ret);
		goto error;
	}
	ret = pthread_create(&rss->thread, NULL, rsem_server_loop, (void*)rss);
	if (ret) {
		snprintf(err, err_len, "pthread_create failed with "
			 "error %d\n", ret);
		goto error;
	}
	return rss;

error:
	rsem_server_free(rss);
	return NULL;
}

void rsem_server_shutdown(struct rsem_server *rss)
{
	int POSSIBLY_UNUSED(res);
	char buf[1] = { 0 };
	res = safe_write(rss->event_fd[PIPE_WRITE], buf, 1);
	pthread_join(rss->thread, NULL);
	rsem_server_free(rss);
}
