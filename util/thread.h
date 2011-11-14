/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_THREAD_CTX_DOT_H
#define REDFISH_UTIL_THREAD_CTX_DOT_H

#include <pthread.h> /* for pthread_t */
#include <stdint.h> /* for uint32_t, etc */

struct fast_log_buf;
struct redfish_thread;

typedef int (*redfish_thread_fn_t)(struct redfish_thread*);

struct redfish_thread
{
	pthread_t pthread;
	struct fast_log_buf *fb;
	uint32_t thread_id;
	redfish_thread_fn_t fn;
	void *init_data;
	void *init_data2;
};

/** Create a redfish thread
 *
 * @param rt		(out param) thread structure to be initialized
 * @param fn		function to run
 * @param data		some data to pass to the new thread
 * @param data2		some more data to pass to the new thread
 *
 * @return		0 on success; error code otherwise 
 */
extern int redfish_thread_create(struct fast_log_mgr *mgr,
		struct redfish_thread* rt, redfish_thread_fn_t fn,
		void *data, void *data2);

/** Join a Redfish thread
 *
 * @param rt		the thread
 *
 * @return		0 on thread success; error code otherwise 
 */
extern int redfish_thread_join(struct redfish_thread *rt);

#endif
