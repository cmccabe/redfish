/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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
