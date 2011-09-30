/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/daemon.h"
#include "core/glitch_log.h"
#include "osd/io.h"
#include "osd/net.h"
#include "util/error.h"
#include "util/platform/socket.h"
#include "util/safe_io.h"

#include <errno.h>
#include <libev/ev.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>

#define PORT_NO 3033
#define BUFFER_SIZE 1024
#define NUM_OSD_PROC_THREADS 16

#define MAX_OSD_NET_QUEUE_LEN 1024

static pthread_t g_osd_proc_threads[NUM_OSD_PROC_THREADS];

static pthread_mutex_t g_osd_net_queue_lock;

static pthread_cond_t g_osd_net_queue_cond;

static int g_osd_net_queue[MAX_OSD_NET_QUEUE_LEN];

static int g_osd_net_queue_len = -1;

static void handle_request_chunk(int fd)
{
	int ret, res, olen;
	uint32_t ty;
	struct mmm_request_chunk req;
	struct mmm_request_chunk_reply *out;
	struct mmm_request_chunk_nack nack;

	res = safe_read(fd, &req, sizeof(req));
	if (res != sizeof(req)) {
		glitch_log("safe_read of mmm_request_chunk returned %d\n", res);
		return;
	}
	req.chunk_id = be64toh(req.chunk_id);
	req.start = be32toh(req.start);
	req.len = be32toh(req.len);
	if (req.len == 0)
		req.len = MAX_CHUNK_SZ;

	/* FIXME: should be using sendfile here, or at least direct copy to
	 * socket */ 
	out = malloc(sizeof(*out) + req.len);
	if (!out) {
		ret = -ENOMEM;
		goto send_error;
	}
	olen = onechunk_read(req.chunk_id, &out->data, req.len, req.start);
	if (olen < 0) {
		ret = olen;
		free(out);
		goto send_error;
	}
	out->chunk_id = htobe64(req.chunk_id);
	out->len = htobe32(olen);

	ty = htobe32(MMM_REQUEST_CHUNK_REPLY);
	ret = safe_write(fd, &ty, sizeof(ty));
	if (ret) {
		glitch_log("safe_write of MMM_REQUEST_CHUNK_REPLY "
			   "returned %d\n", ret);
		return;
	}
	ret = safe_write(fd, out, sizeof(*out) + olen);
	free(out);
	if (ret) {
		glitch_log("safe_write of mmm_request_chunk_reply "
				"returned %d\n", ret);
		return;
	}
	return;

send_error:
	ty = htobe32(MMM_REQUEST_CHUNK_NACK);
	ret = safe_write(fd, &ty, sizeof(ty));
	if (ret) {
		glitch_log("safe_write of MMM_REQUEST_CHUNK_NACK returned "
			   "%d\n", ret);
		return;
	}
	memset(&nack, 0, sizeof(nack)); 
	nack.chunk_id = htobe64(req.chunk_id);
	nack.err = htobe32(ret);
	ret = safe_write(fd, &nack, sizeof(nack));
	if (ret) {
		glitch_log("safe_write of mmm_request_chunk_nack "
			   "returned %d\n", ret);
		return;
	}
}

static void handle_put_chunk(int fd)
{
	int ret;
	uint32_t ty;
	struct mmm_put_chunk m;
	struct mmm_put_chunk_reply mrep;
	char *min;

	ret = safe_read(fd, &m, sizeof(m));
	if (ret != sizeof(m)) {
		glitch_log("safe_read of mmm_put_chunk returned %d\n", ret);
		return;
	}
	m.chunk_id = be64toh(m.chunk_id);
	m.len = be32toh(m.len);

	/* FIXME: should be using copy_to_fd here, or something */
	min = malloc(m.len);
	if (!min) {
		ret = -ENOMEM;
		goto do_send;
	}
	ret = onechunk_write(m.chunk_id, min, m.len, 0);
	if (ret < 0) {
		free(min);
		goto do_send;
	}
	ret = 0;

do_send:
	ty = htobe32(MMM_PUT_CHUNK_REPLY);
	ret = safe_write(fd, &ty, sizeof(ty));
	if (ret) {
		glitch_log("safe_write of MMM_GET_CHUNK_ACK returned %d\n",
			   ret);
		return;
	}
	memset(&mrep, 0, sizeof(mrep)); 
	mrep.chunk_id = htobe64(m.chunk_id);
	mrep.err = htobe32(ret);
	ret = safe_write(fd, &mrep, sizeof(mrep));
	if (ret) {
		glitch_log("safe_write of mmm_get_chunk_ack returned %d\n",
			   ret);
		return;
	}
}

static void osd_process_conn(int fd)
{
	int ret;
	uint32_t ty;

	ret = safe_read_exact(fd, &ty, sizeof(uint32_t));
	if (ret) {
		glitch_log("safe_read_exact of type returned %d\n", ret);
		return;
	}
	ty = ntohl(ty);
	switch (ty) {
	case MMM_REQUEST_CHUNK:
		handle_request_chunk(fd);
		return;
	case MMM_PUT_CHUNK:
		handle_put_chunk(fd);
		return;
	default:
		glitch_log("osd_process_conn: unknown message type %d\n", ty);
		return;
	}
}

static void accept_cb(POSSIBLY_UNUSED(struct ev_loop *loop),
		struct ev_io *w, int revents)
{
	struct sockaddr_in client_addr;
	socklen_t client_len;
	int ret, fd;

	if (EV_ERROR & revents) {
		glitch_log("accept_cb: got invalid event\n");
		return;
	}
	client_len = sizeof(client_addr);
	fd = accept(w->fd, (struct sockaddr *)&client_addr, &client_len);
	if (fd < 0) {
		ret = -errno;
		glitch_log("accept_cb: error %d\n", ret);
		return;
	}
	pthread_mutex_lock(&g_osd_net_queue_lock);
	if (g_osd_net_queue_len == -1) {
		RETRY_ON_EINTR(ret, close(w->fd));
	}
	else {
		g_osd_net_queue[g_osd_net_queue_len] = w->fd;
		pthread_cond_signal(&g_osd_net_queue_cond);
	}
	pthread_mutex_unlock(&g_osd_net_queue_lock);
}

static void* osd_processor_thread(POSSIBLY_UNUSED(void *v))
{
	// TODO: create fast_log and other thread-local stuff
	while (1) {
		int res, fd;

		pthread_mutex_lock(&g_osd_net_queue_lock);
		while (g_osd_net_queue_len == 0) {
			pthread_cond_wait(&g_osd_net_queue_cond,
						&g_osd_net_queue_lock);
		}
		if (g_osd_net_queue_len == -1) {
			/* shutdown */
			pthread_mutex_unlock(&g_osd_net_queue_lock);
			break;
		}
		fd = g_osd_net_queue[g_osd_net_queue_len - 1];
		g_osd_net_queue[g_osd_net_queue_len - 1] = -1;
		g_osd_net_queue_len--;
		pthread_mutex_unlock(&g_osd_net_queue_lock);
		osd_process_conn(fd);
		RETRY_ON_EINTR(res, close(fd));
	}
	// TODO: destroy fast_log and other thread-local stuff
	return NULL;
}

static void shutdown_osd_proc_threads(int nthreads)
{
	int i;

	pthread_mutex_lock(&g_osd_net_queue_lock);
	g_osd_net_queue_len = -1;
	pthread_cond_broadcast(&g_osd_net_queue_cond);
	pthread_mutex_unlock(&g_osd_net_queue_lock);
	for (i = 0; i < nthreads; ++i) {
		pthread_join(g_osd_proc_threads[i], NULL);
	}
}

static int init_osd_proc_threads(void)
{
	int ret, nthreads;
	
	for (nthreads = 0; nthreads < NUM_OSD_PROC_THREADS; ++nthreads) {
		memset(&g_osd_proc_threads, 0, sizeof(g_osd_proc_threads));
		ret = pthread_create(&g_osd_proc_threads[nthreads], NULL,
					osd_processor_thread, NULL);
		if (ret) {
			glitch_log("init_osd_proc_threads: pthread_create "
				   "failed with error %d\n", ret);
			shutdown_osd_proc_threads(nthreads);
			return ret;
		}
	}
	return 0;
}

static void shutdown_osd_net_queue(void)
{
	pthread_mutex_destroy(&g_osd_net_queue_lock);
	pthread_cond_destroy(&g_osd_net_queue_cond);
	memset(g_osd_net_queue, 0, sizeof(g_osd_net_queue));
	g_osd_net_queue_len = -1;
}

static int init_osd_net_queue(void)
{
	int ret;

	ret = pthread_mutex_init(&g_osd_net_queue_lock, NULL);
	if (ret)
		return ret;
	ret = pthread_cond_init(&g_osd_net_queue_cond, NULL);
	if (ret) {
		pthread_mutex_destroy(&g_osd_net_queue_lock);
		return ret;
	}
	memset(&g_osd_net_queue, 0, sizeof(g_osd_net_queue));
	g_osd_net_queue_len = 0;
	return 0;
}

int osd_main_loop(struct daemon *d)
{
	struct ev_loop *loop = ev_default_loop(0);
	int res, ret, sock = -1;
	struct sockaddr_in addr;
	struct ev_io w_accept;

	ret = init_osd_net_queue();
	if (ret) {
		glitch_log("init_osd_net_queue: failed with error %d\n", ret);
		return ret;
	}
	ret = init_osd_proc_threads();
	if (ret) {
		glitch_log("init_osd_proc_threads: failed with error "
				"%d\n", ret);
		goto do_shutdown_osd_net_queue;
	}
	sock = do_socket(AF_INET, SOCK_STREAM, 0,
			WANT_O_CLOEXEC | WANT_TCP_NODELAY);
	if (sock < 0) {
		glitch_log("do_socket erorr %d\n", sock);
		ret = sock;
		goto do_shutdown_osd_proc_threads;
	}
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(d->port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
		ret = -errno;
		glitch_log("bind error %d\n", ret);
		goto do_close_socket;
	}
	if (listen(sock, 2) < 0) {
		ret = -errno;
		glitch_log("listen error %d\n", ret);
		goto do_close_socket;
	}
	/* Start a watcher to accept client requests */
	ev_io_init(&w_accept, accept_cb, sock, EV_READ);
	ev_io_start(loop, &w_accept);

	/* event loop */
	while (1) {
		ev_loop(loop, 0);
	}
	ret = 0;
do_close_socket:
	if (sock >= 0)
		RETRY_ON_EINTR(res, close(sock));
do_shutdown_osd_proc_threads:
	shutdown_osd_proc_threads(NUM_OSD_PROC_THREADS);
do_shutdown_osd_net_queue:
	shutdown_osd_net_queue();
	return ret;
}

// check out ev_idle_init
