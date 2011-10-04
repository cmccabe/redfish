/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef UTIL_NET_DOT_H
#define UTIL_NET_DOT_H

#include <stdint.h> /* for uint32_t */
#include <unistd.h> /* for size_t */

struct json_object;

extern int blocking_read_json_req(const char *fn, int fd,
				  struct json_object **jo);

extern int blocking_write_json_req(const char *fn, int fd,
				   struct json_object *jo);

extern int do_bind_and_listen(int port, char *err, size_t err_len);

extern int write_u32(const char *fn, int fd, uint32_t u);

#endif
