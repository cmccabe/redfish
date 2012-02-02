/*
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

#include "msg/bsend.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "msg/recv_pool.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/macro.h"

#include <errno.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

STAILQ_HEAD(recv_pool_th, recv_pool_thread);
STAILQ_HEAD(pending_tr, mtran);

struct recv_pool {
	/** recv_pool lock */
	pthread_mutex_t lock;
	/** Condition variable to wait on */
	pthread_cond_t cond;
	/** List of recv_pool threads */
	struct recv_pool_th worker_head;
	/** List of incoming transactors with messages */
	struct pending_tr pending_head;
	/** recv_pool has been cancelled */
	int cancel;
};

struct recv_pool *recv_pool_init(POSSIBLY_UNUSED(struct fast_log_buf *fb))
{
	int ret;
	struct recv_pool *rpool;

	rpool = calloc(1, sizeof(struct recv_pool));
	if (!rpool) {
		ret = -ENOMEM;
		goto error;
	}
	STAILQ_INIT(&rpool->worker_head);
	STAILQ_INIT(&rpool->pending_head);
	ret = pthread_mutex_init(&rpool->lock, NULL);
	if (ret)
		goto error_free_rp;
	ret = pthread_cond_init_mt(&rpool->cond);
	if (ret)
		goto error_destroy_lock;
	return rpool;

error_destroy_lock:
	pthread_mutex_destroy(&rpool->lock);
error_free_rp:
	free(rpool);
error:
	return ERR_PTR(ret);
}

static void recv_pool_cb(POSSIBLY_UNUSED(struct mconn *conn), struct mtran *tr)
{
	struct recv_pool *rpool = tr->priv;

	pthread_mutex_lock(&rpool->lock);
	if (rpool->cancel) {
		pthread_mutex_unlock(&rpool->lock);
		return;
	}
	STAILQ_INSERT_TAIL(&rpool->pending_head, tr, u.pending_entry);
	pthread_cond_signal(&rpool->cond);
	pthread_mutex_unlock(&rpool->lock);
}

void recv_pool_msgr_listen(struct recv_pool *rpool, struct msgr *msgr,
		uint16_t port, char *err, size_t err_len)
{
	struct listen_info linfo;

	memset(&linfo, 0, sizeof(linfo));
	linfo.cb = recv_pool_cb;
	linfo.priv = rpool;
	linfo.port = port;
	msgr_listen(msgr, &linfo, err, err_len);
}

static int recv_pool_thread_trampoline(struct redfish_thread *rt)
{
	int ret;
	struct mtran *tr;
	struct recv_pool_thread *rrt  = (struct recv_pool_thread *)rt;
	struct recv_pool *rpool = rrt->rpool;
	recv_pool_handler_fn_t handler = rrt->handler;

	pthread_mutex_lock(&rpool->lock);
	while (1) {
		if (rpool->cancel) {
			pthread_mutex_unlock(&rpool->lock);
			ret = 0;
			break;
		}
		tr = STAILQ_FIRST(&rpool->pending_head);
		if (!tr) {
			pthread_cond_wait(&rpool->cond, &rpool->lock);
			continue;
		}
		STAILQ_REMOVE_HEAD(&rpool->pending_head, u.pending_entry);
		pthread_mutex_unlock(&rpool->lock);
		ret = handler(rt, tr);
		if (ret)
			break;
		pthread_mutex_lock(&rpool->lock);
	}
	return ret;
}

int recv_pool_thread_create(struct recv_pool *rpool, struct fast_log_mgr *mgr,
		struct recv_pool_thread *rt, recv_pool_handler_fn_t handler,
		void *priv)
{
	int ret;

	/* create the Redfish thread */
	pthread_mutex_lock(&rpool->lock);
	if (rpool->cancel) {
		pthread_mutex_unlock(&rpool->lock);
		return -ECANCELED;
	}
	memset(rt, 0, sizeof(struct recv_pool_thread));
	rt->rpool = rpool;
	rt->handler = handler;
	ret = redfish_thread_create(mgr, (struct redfish_thread*)rt,
			recv_pool_thread_trampoline, priv);
	if (ret) {
		pthread_mutex_unlock(&rpool->lock);
		return ret;
	}
	STAILQ_INSERT_TAIL(&rpool->worker_head, rt, entry);
	pthread_mutex_unlock(&rpool->lock);
	return 0;
}

void recv_pool_join(struct recv_pool *rpool)
{
	int ret;
	struct recv_pool_thread *rt;

	pthread_mutex_lock(&rpool->lock);
	rpool->cancel = 1;
	pthread_cond_broadcast(&rpool->cond);
	pthread_mutex_unlock(&rpool->lock);

	while (1) {
		rt = STAILQ_FIRST(&rpool->worker_head);
		if (!rt)
			break;
		ret = redfish_thread_join((struct redfish_thread*)rt);
		STAILQ_REMOVE_HEAD(&rpool->worker_head, entry);
	}
}

void recv_pool_free(struct recv_pool *rp)
{
	pthread_cond_destroy(&rp->cond);
	pthread_mutex_destroy(&rp->lock);
	free(rp);
}
