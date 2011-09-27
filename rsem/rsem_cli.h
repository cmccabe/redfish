/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_RSEM_RSEM_CLI_DOT_H
#define ONEFISH_RSEM_RSEM_CLI_DOT_H

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
