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
#include "util/fast_log.h"
#include "util/macro.h"

#include <errno.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define RECV_POOL_MAX_BSEND_TR 100000

STAILQ_HEAD(pending_tr, mtran);

struct recv_pool {
	/** recv_pool lock */
	pthread_mutex_t lock;
	/** Condition variable to wait on */
	pthread_cond_t cond;
	/** List of incoming transactors with messages */
	struct pending_tr pending_head;
	/** recv_pool has been cancelled */
	int cancel;
	/** Name of receive pool */
	const char *name;
	/** Number of threads */
	int num_threads;
	/** Array of pointers to threads */
	struct recv_pool_thread **threads;
};

struct recv_pool *recv_pool_init(const char *name)
{
	int ret;
	struct recv_pool *rpool;

	rpool = calloc(1, sizeof(struct recv_pool));
	if (!rpool) {
		ret = -ENOMEM;
		goto error;
	}
	rpool->name = name;
	rpool->num_threads = 0;
	rpool->threads = 0;
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
	struct bsend *ctx;
	char fb_name[FAST_LOG_BUF_NAME_MAX];

	snprintf(fb_name, sizeof(fb_name), "%s%d", rpool->name, rt->thread_id);
	fast_log_set_name(rt->fb, fb_name);
	ctx = bsend_init(rt->fb, RECV_POOL_MAX_BSEND_TR);
	if (IS_ERR(ctx)) {
		return PTR_ERR(ctx);
	}
	rrt->ctx = ctx;
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
		ret = handler(rrt, tr);
		if (ret)
			break;
		pthread_mutex_lock(&rpool->lock);
	}
	bsend_free(ctx);
	return ret;
}

int recv_pool_thread_create(struct recv_pool *rpool, struct fast_log_mgr *mgr,
		recv_pool_handler_fn_t handler, void *priv)
{
	int ret;
	struct recv_pool_thread **threads;
	struct recv_pool_thread *rt;

	/* create the Redfish thread */
	pthread_mutex_lock(&rpool->lock);
	if (rpool->cancel) {
		pthread_mutex_unlock(&rpool->lock);
		return -ECANCELED;
	}
	threads = realloc(rpool->threads,
		sizeof (struct recv_pool_thread*) * rpool->num_threads + 1);
	if (!threads) {
		pthread_mutex_unlock(&rpool->lock);
		return -ENOMEM;
	}
	rpool->threads = threads;
	rt = calloc(1, sizeof(struct recv_pool_thread));
	if (!rt) {
		pthread_mutex_unlock(&rpool->lock);
		return -ENOMEM;
	}
	threads[rpool->num_threads] = rt;
	memset(rt, 0, sizeof(struct recv_pool_thread));
	rt->rpool = rpool;
	rt->handler = handler;
	ret = redfish_thread_create(mgr, (struct redfish_thread*)rt,
			recv_pool_thread_trampoline, priv);
	if (ret) {
		free(rt);
		pthread_mutex_unlock(&rpool->lock);
		return ret;
	}
	rpool->num_threads++;
	pthread_mutex_unlock(&rpool->lock);
	return 0;
}

void recv_pool_join(struct recv_pool *rpool)
{
	int i, POSSIBLY_UNUSED(ret);

	pthread_mutex_lock(&rpool->lock);
	rpool->cancel = 1;
	pthread_cond_broadcast(&rpool->cond);
	pthread_mutex_unlock(&rpool->lock);

	for (i = 0; i < rpool->num_threads; ++i) {
		ret = redfish_thread_join((struct redfish_thread*)
			rpool->threads[i]);
	}
}

void recv_pool_free(struct recv_pool *rp)
{
	pthread_cond_destroy(&rp->cond);
	pthread_mutex_destroy(&rp->lock);
	free(rp->threads);
	free(rp);
}
