/*
 * vim: ts=8:sw=8:tw=79:noet
 * 
 * Copyright 2012 the Redfish authors
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

#ifndef REDFISH_MSG_RECV_POOL_H
#define REDFISH_MSG_RECV_POOL_H

/*
 * Redfish blocking RPC implementation
 */

#include "util/thread.h" /* for struct redfish_thread */

struct fast_log_buf;
struct msgr;
struct mtran;
struct recv_pool;
struct recv_pool_thread;

typedef int (*recv_pool_handler_fn_t)(struct recv_pool_thread *rt,
			struct mtran *tr);

struct recv_pool_thread {
	struct redfish_thread base;
	struct recv_pool *rpool;
	recv_pool_handler_fn_t handler;
	struct bsend *ctx;
};

/** Create an RPC receive thread pool
 *
 * @param name		Name of the receive pool.  This will be deep-copied.
 *
 * @return		Pointer to a valid recv_pool, or an error pointer
 */
extern struct recv_pool *recv_pool_init(const char *name);

/** Hook up a messenger to a receive pool
 *
 * @param rpool		The receive pool
 * @param msgr		The messenger to hook up
 * @param port		The port to use
 * @param err		(out param) error message on failure
 * @param err_len	length of err buffer
 */
extern void recv_pool_msgr_listen(struct recv_pool *rpool, struct msgr *msgr,
		uint16_t port, char *err, size_t err_len);

/** Add a thread to a receive pool
 *
 * @param rpool		The receive pool
 * @param mgr		The fast log manager to use when creating this thread
 * @param handler	The handler function to invoke on each RPC
 * @param priv		Pointer to put in rt->base.priv
 *
 * @return		0 on success; error code otherwise
 */
extern int recv_pool_thread_create(struct recv_pool *rpool,
	struct fast_log_mgr *mgr, recv_pool_handler_fn_t handler, void *priv);

/** Join all threads in a receive pool
 *
 * @rpool		The receive pool
 */
extern void recv_pool_join(struct recv_pool *rpool);

/** Free the memory associated with a receive pool
 *
 * @rpool		The receive pool
 */
extern void recv_pool_free(struct recv_pool *rpool);

#endif
