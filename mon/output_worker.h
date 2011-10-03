/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_OUTPUT_WORKER_DOT_H
#define REDFISH_OUTPUT_WORKER_DOT_H

#define LBUF_LEN_DIGITS 8

/** Initialize the output worker.
 *
 * @param sock_path	path to the socket to listen on
 * @param err		The error output buffer
 * @param err_len	The length of the error output buffer
 */
void init_output_worker(const char* sock_path, char *err, size_t err_len);

/** Tell the update worker that the state has changed and observers need to be
 * updated
 */
void kick_output_worker(void);

/** Tell the update worker to shut itself down.
 *
 * This should be done only after all the threads calling kick_output_worker
 * have been stopped.
 */
void shutdown_output_worker(void);

#endif
