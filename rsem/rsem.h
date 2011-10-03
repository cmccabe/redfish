/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_RSEM_RSEM_DOT_H
#define REDFISH_RSEM_RSEM_DOT_H

#include <stdint.h> /* for uint32_t */
#include <unistd.h> /* for size_t */

#define JORM_CUR_FILE "rsem/rsem.jorm"
#include "jorm/jorm_generate_include.h"
#undef JORM_CUR_FILE

#if 0 /* Give the dependency scanner a clue */
#include "rsem/rsem.jorm"
#endif

/* Network messages we send and receive.
 * Some of them have corresponding JSON payloads. */
enum {
	RSEM_CLIENT_ACK,
	RSEM_CLIENT_REL_SEM,
	RSEM_CLIENT_REQ_SEM,
	RSEM_SERVER_ACK,
	RSEM_SERVER_NACK,
	RSEM_SERVER_DELAY_SEM,
	RSEM_SERVER_GIVE_SEM,
	RSEM_SERVER_INTERNAL_ERROR,
	RSEM_SERVER_NO_SUCH_SEM,
};

struct json_object;

extern int blocking_read_json_req(const char *fn, int fd,
				  struct json_object **jo);

extern int blocking_write_json_req(const char *fn, int fd,
				   struct json_object *jo);

extern int do_bind_and_listen(int port, char *err, size_t err_len);

extern int write_u32(const char *fn, int fd, uint32_t u);

#endif
