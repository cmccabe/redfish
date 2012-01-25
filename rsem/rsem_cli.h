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

#ifndef REDFISH_RSEM_RSEM_CLI_DOT_H
#define REDFISH_RSEM_RSEM_CLI_DOT_H

#include <unistd.h> /* for size_t */

struct rsem_client_conf;

struct rsem_client;

/** Initialize a remote semaphore client
 *
 * @param conf		The client configuration
 * @param err		Error buffer
 * @param err_len	Length of the error buffer
 *
 * @return		The remote sempahore client on success; NULL otherwise
 */
extern struct rsem_client* rsem_client_init(struct rsem_client_conf *conf,
					    char *err, size_t err_len);

/** Destroy a remote semaphore client
 *
 * @param rcli		The client
 */
extern void rsem_client_destroy(struct rsem_client* rcli);

/** Release a remote semaphore
 *
 * @param rcli		The client
 * @param name		The remote semaphore to release
 */
extern void rsem_post(struct rsem_client *rcli, const char *name);

/** Acquire a remote semaphore
 *
 * @param rcli		The client
 * @param name		The remote semaphore to acquire
 */
extern int rsem_wait(struct rsem_client *rcli, const char *name);

#endif
