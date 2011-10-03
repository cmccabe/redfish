/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/glitch_log.h"
#include "core/log_config.h"
#include "rsem/rsem.h"
#include "util/error.h"
#include "util/platform/socket.h"
#include "util/safe_io.h"

#define JORM_CUR_FILE "rsem/rsem.jorm"
#include "jorm/jorm_generate_body.h"
#undef JORM_CUR_FILE

#include <arpa/inet.h>
#include <json/json.h>

int blocking_read_json_req(const char *fn, int fd, struct json_object **jo)
{
	char err[128] = { 0 };
	struct json_object *zjo;
	uint32_t len;
	ssize_t res;
	char *b;

	res = safe_read(fd, &len, sizeof(uint32_t));
	if (res != sizeof(uint32_t)) {
		glitch_log("%s: error reading JSON length: error %Zd\n",
			   fn, res);
		return -EIO;
	}
	len = ntohl(len);
	b = malloc(len + 1);
	if (!b)
		return -ENOMEM;
	if (safe_read(fd, b, len) != len) {
		glitch_log("%s: short read of msg body\n", fn);
		free(b);
		return -EIO;
	}
	b[len] = '\0';
	zjo = parse_json_string(b, err, sizeof(err));
	free(b);
	if (err[0]) {
		glitch_log("%s: invalid JSON: %s\n", fn, err);
		return -EINVAL;
	}
	*jo = zjo;
	return 0;
}

int blocking_write_json_req(const char *fn, int fd, struct json_object *jo)
{
	const char *buf = json_object_to_json_string(jo);
	uint32_t b_len = strlen(buf);
	uint32_t n_len = htonl(b_len);

	if (safe_write(fd, &n_len, sizeof(uint32_t))) {
		glitch_log("%s: short write of len\n", fn);
		return -EIO;
	}
	if (safe_write(fd, buf, b_len)) {
		glitch_log("%s: short write of msg body\n", fn);
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

int write_u32(const char *fn, int fd, uint32_t u)
{
	int ret;
	uint32_t eu = htonl(u);

	ret = safe_write(fd, &eu, sizeof(uint32_t));
	if (ret) {
		glitch_log("%s: short write of %d\n", fn, u);
		return ret;
	}
	return 0;
}
