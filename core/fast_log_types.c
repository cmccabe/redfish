/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */


#include "core/fast_log.h"
#include "core/fast_log_types.h"
#include "util/compiler.h"
#include "util/macro.h"

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
	FAST_LOG_TYPE_NEW_CONN = 1,
	FAST_LOG_TYPE_CLOSE_CONN
};

PACKED_ALIGNED(8,
struct fast_log_new_conn_entry
{
        /** The fast_log message type */
	uint16_t type;
	/** Type of new connection */
	uint32_t conn_ty;
	/** New file descriptor */
	uint32_t fd;
	/** Source address */
	unsigned long s_addr;
}
);

BUILD_BUG_ON(sizeof(struct fast_log_new_conn_entry) >
		sizeof(struct fast_log_entry)); 

void fast_log_new_conn(struct fast_log_buf *fb, uint32_t conn_ty,
			uint32_t fd, unsigned long s_addr)
{
	union {
		struct fast_log_new_conn_entry f;
		struct fast_log_entry fe;
	} fe;
	memset(&fe.fe, 0, sizeof(fe.fe));
	fe.f.type = FAST_LOG_TYPE_NEW_CONN;
	fe.f.conn_ty = conn_ty;
	fe.f.fd = fd;
	fe.f.s_addr = s_addr;
	fast_log(fb, &fe.fe);
}

static int fast_log_new_conn_dump(struct fast_log_entry *fe, int fd)
{
	char str[INET_ADDRSTRLEN];
	struct fast_log_new_conn_entry *f =
		(struct fast_log_new_conn_entry*)fe;
	inet_ntop(AF_INET, &f->s_addr, str, INET_ADDRSTRLEN);
	return dprintf(fd, "new_conn(conn_ty=%d,fd=%d,s_addr=%s)\n",
			f->conn_ty, f->fd, str);
}

PACKED_ALIGNED(8,
struct fast_log_close_conn_entry
{
        /** The fast_log message type */
	uint16_t type;
	/** Type of new connection */
	uint32_t conn_ty;
	/** New file descriptor */
	uint32_t fd;
	/** Source address */
	unsigned long s_addr;
	/** Error, if any */
	int32_t err;
}
);

BUILD_BUG_ON(sizeof(struct fast_log_close_conn_entry) >
		sizeof(struct fast_log_entry)); 

void fast_log_close_conn(struct fast_log_buf *fb, uint32_t conn_ty,
			uint32_t fd, unsigned long s_addr)
{
	union {
		struct fast_log_close_conn_entry f;
		struct fast_log_entry fe;
	} fe;
	memset(&fe.fe, 0, sizeof(fe.fe));
	fe.f.type = FAST_LOG_TYPE_CLOSE_CONN;
	fe.f.conn_ty = conn_ty;
	fe.f.fd = fd;
	fe.f.s_addr = s_addr;
	fast_log(fb, &fe.fe);
}

static int fast_log_close_conn_dump(struct fast_log_entry *fe, int fd)
{
	char str[INET_ADDRSTRLEN];
	struct fast_log_close_conn_entry *f =
		(struct fast_log_close_conn_entry*)fe;
	inet_ntop(AF_INET, &f->s_addr, str, INET_ADDRSTRLEN);
	return dprintf(fd, "closee_conn(conn_ty=%d,fd=%d,s_addr=%s,err=%d)\n",
		f->conn_ty, f->fd, str, f->err);
}

const fast_log_dumper_fn_t g_fast_log_dumpers[] = {
	[FAST_LOG_TYPE_NEW_CONN] = fast_log_new_conn_dump,
	[FAST_LOG_TYPE_CLOSE_CONN] = fast_log_close_conn_dump,
};
