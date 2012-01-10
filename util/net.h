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

extern void ipv4_to_str(uint32_t addr, char *out, size_t out_len);

extern uint32_t get_first_ipv4_addr(const char *host, char *err, size_t err_len);

#endif
