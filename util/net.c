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

#include "util/net.h"
#include "jorm/json.h"
#include "util/error.h"
#include "util/platform/socket.h"
#include "util/safe_io.h"

#include <arpa/inet.h>
#include <errno.h>
#include <json/json.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

int blocking_read_json_req(POSSIBLY_UNUSED(const char *fn),
			int fd, struct json_object **jo)
{
	char err[128] = { 0 };
	struct json_object *zjo;
	uint32_t len;
	ssize_t res;
	char *b;

	res = safe_read(fd, &len, sizeof(uint32_t));
	if (res != sizeof(uint32_t)) {
//		glitch_log("%s: error reading JSON length: error %Zd\n",
//			   fn, res);
		return -EIO;
	}
	len = ntohl(len);
	b = malloc(len + 1);
	if (!b)
		return -ENOMEM;
	if ((uint32_t)safe_read(fd, b, len) != len) {
		//glitch_log("%s: short read of msg body\n", fn);
		free(b);
		return -EIO;
	}
	b[len] = '\0';
	zjo = parse_json_string(b, err, sizeof(err));
	free(b);
	if (err[0]) {
		//glitch_log("%s: invalid JSON: %s\n", fn, err);
		return -EINVAL;
	}
	*jo = zjo;
	return 0;
}

int blocking_write_json_req(POSSIBLY_UNUSED(const char *fn),
		int fd, struct json_object *jo)
{
	const char *buf = json_object_to_json_string(jo);
	uint32_t b_len = strlen(buf);
	uint32_t n_len = htonl(b_len);

	if (safe_write(fd, &n_len, sizeof(uint32_t))) {
		//glitch_log("%s: short write of len\n", fn);
		return -EIO;
	}
	if (safe_write(fd, buf, b_len)) {
		//glitch_log("%s: short write of msg body\n", fn);
		return -EIO;
	}
	return 0;
}

int do_bind_and_listen(int port, char *err, size_t err_len)
{
	int ret, zfd;
	struct sockaddr_in address;
	zfd = do_socket(AF_INET, SOCK_STREAM, 0, WANT_O_CLOEXEC);
	if (zfd < 0) {
		ret = zfd;
		snprintf(err, err_len,
			 "failed to create socket: error %d", ret);
		goto error;
	}
	memset(&address, 0, sizeof(struct sockaddr_in));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	ret = 0;
	if (bind(zfd, (struct sockaddr*)&address,
			sizeof(struct sockaddr_in))) {
		ret = errno;
		snprintf(err, err_len, "Error binding to port %d: error %d",
			port, ret);
		goto error_close_sock;
	}
	if (listen(zfd, 5)) {
		ret = errno;
		snprintf(err, err_len, "Failed to listen to port %d: error %d",
			port, ret);
		goto error_close_sock;
	}
	return zfd;

error_close_sock:
	RETRY_ON_EINTR(ret, close(zfd));
error:
	return -1;
}

int write_u32(POSSIBLY_UNUSED(const char *fn), int fd, uint32_t u)
{
	int ret;
	uint32_t eu = htonl(u);

	ret = safe_write(fd, &eu, sizeof(uint32_t));
	if (ret) {
		//glitch_log("%s: short write of %d\n", fn, u);
		return ret;
	}
	return 0;
}

void ipv4_to_str(uint32_t addr, char *out, size_t out_len)
{
	addr = htonl(addr);
	inet_ntop(AF_INET, &addr, out, out_len);
}

uint32_t get_first_ipv4_addr(const char *host, char *err, size_t err_len)
{
	uint32_t a = 0;
	int ret;
	struct addrinfo hints, *res, *r;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags |= AI_CANONNAME;
	ret = getaddrinfo(host, NULL, &hints, &res);
	if (ret) {
		if (ret == EAI_SYSTEM)
			ret = errno;
		snprintf(err, err_len, "getaddrinfo(%s): error %d", host, ret);
		return 0;
	}
	for (r = res; r; r = r->ai_next) {
		if (r->ai_family != AF_INET)
			continue;
		a = ((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
		goto done;
	}
	snprintf(err, err_len, "getaddrinfo(%s): no IPv4 addresses "
		 "found!", host);
done:
	freeaddrinfo(res);
	return a;
}
