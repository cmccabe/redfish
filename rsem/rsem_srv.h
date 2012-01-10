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

#ifndef REDFISH_RSEM_RSEM_SRV_DOT_H
#define REDFISH_RSEM_RSEM_SRV_DOT_H

#include <unistd.h> /* for size_t */

struct rsem_server;
struct rsem_server_conf;

/** Start a lock server
 *
 * @param conf		The lock server configuration to use
 * @param err		The error buffer
 * @param err_len	Length of the error buffer
 *
 * @return		A lock server, or NULL on error.
 */
extern struct rsem_server* start_rsem_server(struct rsem_server_conf *conf,
						char *err, size_t err_len);

/** Shut down the lock server
 *
 * @param lsd		The lock server to shut down
 */
extern void rsem_server_shutdown(struct rsem_server *rss);

#endif
