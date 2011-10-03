/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/glitch_log.h"
#include "rsem/rsem.h"
#include "rsem/rsem_cli.h"
#include "util/error.h"
#include "util/msleep.h"
#include "util/platform/socket.h"
#include "util/safe_io.h"

#include <arpa/inet.h>
#include <errno.h>
#include <json/json.h>
#include <netdb.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CLI_PORTS 1000

struct rsem_client {
	char *srv_host;
	int srv_port;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int num_avail_ports;
	int avail_ports[0];
};

struct rsem_client* rsem_client_init(struct rsem_client_conf *conf,
					    char *err, size_t err_len)
{
	int i, num_avail_ports;
	struct rsem_client* rcli;

	if (conf->cli_port_start > conf->cli_port_end) {
		snprintf(err, err_len, "rsem_client_init: invalid "
			"configuration. cli_port_start was %d, but "
			"cli_port_end was %d!",
			conf->cli_port_start, conf->cli_port_end);
		goto error;
	}
	num_avail_ports = 1 + conf->cli_port_end - conf->cli_port_start;
	if (num_avail_ports > MAX_CLI_PORTS) {
		snprintf(err, err_len, "rsem_client_init: invalid "
			"configuration. can't allocate more than %d client "
			"ports.", MAX_CLI_PORTS);
		goto error;
	}
	rcli = calloc(1, sizeof(struct rsem_client) +
		      (sizeof(int) * num_avail_ports));
	if (!rcli) {
		snprintf(err, err_len, "rsem_client_init: out of memory");
		goto error;
	}
	rcli->num_avail_ports = num_avail_ports;
	for (i = 0; i < num_avail_ports; ++i) {
		rcli->avail_ports[i] = conf->cli_port_end - i;
	}
	rcli->srv_host = strdup(conf->srv_host);
	if (!rcli->srv_host) {
		snprintf(err, err_len, "rsem_client_init: out of memory");
		goto error_free_rcli;
	}
	rcli->srv_port = conf->srv_port;
	if (pthread_mutex_init(&rcli->lock, NULL)) {
		snprintf(err, err_len, "rsem_client_init: "
			 "pthread_mutex_init failed.");
		goto error_free_rcli_host;
	}
	if (pthread_cond_init(&rcli->cond, NULL)) {
		snprintf(err, err_len, "rsem_client_init: "
			 "pthread_mutex_init failed.");
		goto error_destroy_mutex;
	}
	return rcli;

error_destroy_mutex:
	pthread_mutex_destroy(&rcli->lock);
error_free_rcli_host:
	free(rcli->srv_host);
error_free_rcli:
	free(rcli);
error:
	return NULL;
}

void rsem_client_destroy(struct rsem_client* rcli)
{
	free(rcli->srv_host);
	pthread_mutex_destroy(&rcli->lock);
	pthread_cond_destroy(&rcli->cond);
	free(rcli);
}

static long int get_first_ipv4_addr(const char *fn, const char *host)
{
	int ret;
	long int a = 0;
	struct addrinfo hints, *res, *r;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags |= AI_CANONNAME;
	ret = getaddrinfo(host, NULL, &hints, &res);
	if (ret) {
		glitch_log("%s: getaddrinfo error %d\n", fn, ret);
		return ret;
	}
	for (r = res; r; r = r->ai_next) {
		if (r->ai_family != AF_INET)
			continue;
		a = ((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
		break;
	}
	freeaddrinfo(res);
	return a;
}

static int rsem_post_impl(struct rsem_client *rcli, const char *name)
{
	int res, ret, zfd;
	struct json_object *jo = NULL;
	struct sockaddr_in addr;
	uint32_t ty, resp;
	struct rsem_release rel;

	rel.name = (char*)name;
	jo = JORM_TOJSON_rsem_release(&rel);
	if (!jo) {
		glitch_log("rsem_post_impl: out of memory\n");
		goto done;
	}
	zfd = do_socket(AF_INET, SOCK_STREAM, 0, WANT_O_CLOEXEC);
	if (zfd < 0) {
		ret = zfd;
		glitch_log("rsem_post_impl: socket error: %d\n", ret);
		goto done;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = get_first_ipv4_addr("rsem_post_impl",
						   rcli->srv_host);
	if (!addr.sin_addr.s_addr) {
		/* couldn't resolve hostname */
		ret = -EIO;
		goto done;
	}
	addr.sin_port = htons(rcli->srv_port);
	if (connect(zfd, &addr, sizeof(addr)) < 0) {
		ret = errno;
		glitch_log("rsem_post_impl: failed to connect to %s: "
			   "error %d\n", rcli->srv_host, ret);
		ret = -EIO;
		goto done;
	}
	ty = htonl(RSEM_CLIENT_REL_SEM);
	if (safe_write(zfd, &ty, sizeof(uint32_t))) {
		glitch_log("rsem_post_impl: short write of message type %d\n",
			   RSEM_CLIENT_REL_SEM);
		ret = -EIO;
		goto done;
	}
	if (blocking_write_json_req("rsem_post_impl", zfd, jo)) {
		goto done;
	}
	if (safe_read(zfd, &resp, sizeof(uint32_t)) != sizeof(uint32_t)) {
		glitch_log("rsem_post_impl: short read of response\n");
		ret = -EIO;
		goto done;
	}
	resp = ntohl(resp);
	if (resp == RSEM_SERVER_ACK) {
		ret = 0;
	}
	else {
		glitch_log("rsem_post_impl: got unexpected server "
			   "response %d\n", resp);
		ret = -EIO;
	}

done:
	if (jo)
		json_object_put(jo);
	if (zfd >= 0)
		RETRY_ON_EINTR(res, close(zfd));
	return ret;
}

void rsem_post(struct rsem_client *rcli, const char *name)
{
	/* We keep spinning until we can inform the server about the semaphore
	 * we've just released.
	 */
	while (1) {
		int ret = rsem_post_impl(rcli, name);
		if (ret == 0)
			return;
		do_msleep(1000);
	}
}

static int wait_for_next_free_port(struct rsem_client *rcli)
{
	int port;

	pthread_mutex_lock(&rcli->lock);
	while (rcli->num_avail_ports == 0) {
		pthread_cond_wait(&rcli->cond, &rcli->lock);
	}
	port = rcli->avail_ports[rcli->num_avail_ports - 1];
	rcli->avail_ports[rcli->num_avail_ports - 1] = -1;
	rcli->num_avail_ports--;
	pthread_mutex_unlock(&rcli->lock);
	return port;
}

static void release_port(struct rsem_client *rcli, int port)
{
	pthread_mutex_lock(&rcli->lock);
	rcli->num_avail_ports++;
	rcli->avail_ports[rcli->num_avail_ports - 1] = port;
	pthread_cond_signal(&rcli->cond);
	pthread_mutex_unlock(&rcli->lock);
}

static int rsem_wait_for_callback_impl(const char *name, int fd)
{
	struct json_object *jo;
	struct rsem_grant *grant;
	uint32_t ty;

	if (safe_read(fd, &ty, sizeof(uint32_t)) != sizeof(uint32_t)) {
		glitch_log("rsem_wait_for_callback_impl: short read "
			   "of type\n");
		return -EIO;
	}
	ty = ntohl(ty);
	if (ty != RSEM_SERVER_GIVE_SEM) {
		glitch_log("rsem_wait_for_callback_impl: got unexpected "
			   "response %d from server\n", ty);
		return -EIO;
	}
	if (blocking_read_json_req("rsem_wait_for_callback", fd, &jo))
		return -EIO;
	grant = JORM_FROMJSON_rsem_grant(jo);
	if (!grant) {
		glitch_log("rsem_wait_for_callback_impl: failed to decode "
			"RSEM_SERVER_GIVE_SEM reply\n");
		json_object_put(jo);
		return -EIO;
	}
	json_object_put(jo);
	if (strcmp(grant->name, name)) {
		glitch_log("rsem_wait_for_callback_impl: server granted us "
			"'%s', but we requested '%s'\n", grant->name, name);
		JORM_FREE_rsem_grant(grant);
		return -EIO;
	}
	JORM_FREE_rsem_grant(grant);
	ty = htonl(RSEM_CLIENT_ACK);
	if (safe_write(fd, &ty, sizeof(uint32_t))) {
		glitch_log("rsem_wait_for_callback_impl: short write "
			   "of ack\n");
		return -EIO;
	}
	return 0;
}

static int rsem_wait_for_callback(const char *name, int zsock)
{
	int res, ret, fd;
	struct sockaddr_in addr;
	socklen_t addr_len;

	addr_len = sizeof(addr);
	fd = accept(zsock, (struct sockaddr *)&addr, &addr_len);
	if (fd < 0) {
		int ret = -errno;
		glitch_log("rsem_wait_for_callback: accept error %d\n", ret);
		return -EIO;
	}
	ret = rsem_wait_for_callback_impl(name, fd);
	RETRY_ON_EINTR(res, close(fd));
	return ret;
}

int rsem_wait(struct rsem_client *rcli, const char *name)
{
	char err[512] = { 0 };
	int ret, res, port, zfd = -1, zsock = -1;
	struct rsem_request req;
	struct json_object *jo;
	uint32_t ty, resp;
	struct sockaddr_in addr;

	port = wait_for_next_free_port(rcli);
	zsock = do_bind_and_listen(port, err, sizeof(err));
	if (err[0]) {
		glitch_log("rsem_wait: do_bind_and_listen failed with "
			   "error '%s'\n", err);
		ret = -EIO;
		zsock = -1;
		goto done;
	}
	zfd = do_socket(AF_INET, SOCK_STREAM, 0, WANT_O_CLOEXEC);
	if (zfd < 0) {
		glitch_log("rsem_wait: socket error: %d\n", zfd);
		ret = EIO;
		goto done;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = get_first_ipv4_addr("rsem_wait",rcli->srv_host);
	if (!addr.sin_addr.s_addr) {
		/* couldn't resolve hostname */
		ret = EIO;
		goto done;
	}
	addr.sin_port = htons(rcli->srv_port);
	if (connect(zfd, &addr, sizeof(addr)) < 0) {
		ret = errno;
		glitch_log("rsem_wait: failed to connect to %s: error %d\n",
			   rcli->srv_host, ret);
		goto done;
	}
	ty = htonl(RSEM_CLIENT_REQ_SEM);
	if (safe_write(zfd, &ty, sizeof(uint32_t))) {
		glitch_log("rsem_wait: short write of message type\n");
		ret = -EIO;
		goto done;
	}
	memset(&req, 0, sizeof(req));
	req.name = (char*)name;
	req.port = port;
	jo = JORM_TOJSON_rsem_request(&req);
	if (!jo) {
		glitch_log("rsem_wait: out of memory\n");
		goto done;
	}
	if (blocking_write_json_req("rsem_wait", zfd, jo)) {
		json_object_put(jo);
		goto done;
	}
	json_object_put(jo);
	if (safe_read(zfd, &resp, sizeof(uint32_t)) != sizeof(uint32_t)) {
		glitch_log("rsem_wait: short read of response\n");
		ret = -EIO;
		goto done;
	}
	resp = ntohl(resp);
	RETRY_ON_EINTR(res, close(zfd));
	zfd = -1;
	if (resp == RSEM_SERVER_GIVE_SEM) {
		ret = 0;
	}
	else if (resp == RSEM_SERVER_DELAY_SEM) {
		ret = rsem_wait_for_callback(name, zsock);
	}
	else {
		glitch_log("rsem_wait: unexpected server reply %d\n",
				resp);
		ret = -EIO;
	}

done:
	if (zsock >= 0)
		RETRY_ON_EINTR(res, close(zsock));
	if (zfd >= 0)
		RETRY_ON_EINTR(res, close(zfd));
	release_port(rcli, port);
	return ret;
}
