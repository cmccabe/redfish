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
#include "util/compiler.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_types.h"
#include "util/macro.h"
#include "util/thread.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct bsend_mtran {
	struct mtran *tr;
	struct bsend *ctx;
	uint8_t flags;
};

/** A blocking RPC context */
struct bsend {
	/** Protects num_finished and cancel */
	pthread_mutex_t lock;
	/** Condition variable to wait for completion of RPCs */
	pthread_cond_t cond;
	/** Fast log buffer we use for operations */
	struct fast_log_buf *fb;
	/** Array of pointers to bsend transactors */
	struct bsend_mtran *btrs;
	/** Length of btrs */
	int max_tr;
	/** Current number of transactions */
	int num_tr;
	/** Transaction timeout, in seconds */
	int timeout;
	/** If nonzero, the context has been cancelled. */
	int cancel;
	/** Number of finished transactors */
	int num_finished;
};

struct bsend *bsend_init(struct fast_log_buf *fb, int max_tr, int timeout)
{
	int ret;
	struct bsend *ctx;
	struct bsend_mtran *btrs;

	ctx = calloc(1, sizeof(struct bsend));
	if (!ctx) {
		ret = ENOMEM;
		goto error;
	}
	ctx->fb = fb;
	btrs = calloc(max_tr, sizeof(struct bsend_mtran));
	if (!btrs) {
		ret = ENOMEM;
		goto error_free_ctx;
	}
	ctx->max_tr = max_tr;
	ctx->btrs = btrs;
	ctx->num_tr = 0;
	ctx->timeout = timeout;
	ret = pthread_mutex_init(&ctx->lock, NULL);
	if (ret)
		goto error_free_btrs;
	ret = pthread_cond_init_mt(&ctx->cond);
	if (ret)
		goto error_destroy_lock;
	fast_log_bsend(ctx->fb, initialized with max_tr = , timeout = )
	return ctx;

error_destroy_lock:
	pthread_mutex_destroy(&ctx->lock);
error_free_btrs:
	free(btrs);
error_free_ctx:
	free(ctx);
error:
	return ERR_PTR(FORCE_POSITIVE(ret));
}

static void bsend_cb_complete(struct bsend *ctx)
{
	pthread_mutex_lock(&ctx->lock);
	++ctx->num_finished;
	if (ctx->num_finished == ctx->num_tr)
		pthread_cond_signal(&ctx->cond);
	pthread_mutex_unlock(&ctx->lock);
}

static void bsend_cb(struct mconn *conn, struct mtran *tr)
{
	struct bsend_mtran *btr = (struct bsend_mtran *)tr->priv;
	struct bsend *ctx = btr->ctx;

//	fprintf(stderr, "bsend_cb in state %s\n",
//		mtran_state_to_str(tr->state)); 
	if (btr->flags & BSF_RESP) {
		/* Let's get the response */
		if (tr->state == MTRAN_STATE_SENT) {
			if (tr->m == NULL)
				mtran_recv_next(conn, tr);
			return;
		}
		else if (tr->state == MTRAN_STATE_RECV) {
			bsend_cb_complete(ctx);
			return;
		}
		else {
			abort();
		}
	}
	else {
		/* We don't expect a response */
		if (tr->state == MTRAN_STATE_SENT) {
			bsend_cb_complete(ctx);
			return;
		}
		else {
			abort();
		}
	}
}

int bsend_add(struct bsend *ctx, struct msgr *msgr, uint8_t flags,
		struct msg *msg, uint32_t addr, uint16_t port)
{
	struct mtran *tr;

	tr = mtran_alloc(msgr);
	if (!tr) {
		fast_log_bsend(ctx->fb, failed to add bsend to addr, port, with flags)
		return -ENOMEM;
	}
	tr->ip = addr;
	tr->port = port;
	return bsend_add_tr_or_free(ctx, msgr, flags, msg, tr);
}

int bsend_add_tr_or_free(struct bsend *ctx, struct msgr *msgr, uint8_t flags,
		struct msg *msg, struct mtran *tr)
{
	struct bsend_mtran *btr;

	if (ctx->num_tr >= ctx->max_tr) {
		mtran_free(tr);
		fast_log_bsend(ctx->fb, failed to add bsend to addr, port, with flags... too many tr)
		return -EMFILE;
	}
	btr = &ctx->btrs[ctx->num_tr];
	btr->tr = tr;
	btr->ctx = ctx;
	btr->flags = flags;
	pthread_mutex_lock(&ctx->lock);
	if (ctx->cancel) {
		pthread_mutex_unlock(&ctx->lock);
		mtran_free(tr);
		fast_log_bsend(ctx->fb, failed to add bsend to addr, port, with flags... canceled)
		return -ECANCELED;
	}
	pthread_mutex_unlock(&ctx->lock);
	ctx->num_tr++;
	fast_log_bsend(ctx->fb, added bsend to addr, port, with flags)
	mtran_send(msgr, tr, bsend_cb, btr, msg);
	return 0;
}

int bsend_join(struct bsend *ctx)
{
	int i;
	struct bsend_mtran *btr;

	pthread_mutex_lock(&ctx->lock);
	while (1) {
//		printf("ctx->cancel = %d, ctx->num_finished = %d, "
//			"ctx->num_tr = %d\n", ctx->cancel,
//			ctx->num_finished, ctx->num_tr);
		if (ctx->cancel) {
			pthread_mutex_unlock(&ctx->lock);
			for (i = 0; i < ctx->num_tr; ++i) {
				btr = &ctx->btrs[i];
				if (!IS_ERR(btr->tr->m)) {
					free(btr->tr->m);
				}
				btr->tr->m = ERR_PTR(ECANCELED);
			}
			fast_log_bsend(ctx->fb, bsend_join canceled)
			return -ECANCELED;
		}
		if (ctx->num_finished == ctx->num_tr) {
			pthread_mutex_unlock(&ctx->lock);
			fast_log_bsend(ctx->fb, joined %d transactors)
			return ctx->num_tr;
		}
		fast_log_bsend(ctx->fb, joined %d transactors, %d to go)
		pthread_cond_wait(&ctx->cond, &ctx->lock);
	}
}

struct mtran *bsend_get_mtran(struct bsend *ctx, int ntr)
{
	if (ntr >= ctx->num_tr)
		return NULL;
	return ctx->btrs[ntr].tr;
}

void bsend_reset(struct bsend *ctx)
{
	int i;

	fast_log_bsend(ctx->fb, bsend reset)
	if (ctx->num_finished != ctx->num_tr)
		abort();
	for (i = 0; i < ctx->num_tr; ++i) {
		mtran_free(ctx->btrs[i].tr);
		ctx->btrs[i].tr = NULL;
		ctx->btrs[i].flags = 0;
	}
	ctx->num_tr = 0;
	ctx->num_finished = 0;
}

void bsend_cancel(struct bsend *ctx)
{
	pthread_mutex_lock(&ctx->lock);
	if (ctx->cancel) {
		pthread_mutex_unlock(&ctx->lock);
		return;
	}
	ctx->cancel = 1;
	ctx->num_finished = ctx->num_tr;
	pthread_cond_signal(&ctx->cond);
	pthread_mutex_unlock(&ctx->lock);
}

void bsend_free(struct bsend *ctx)
{
	pthread_mutex_destroy(&ctx->lock);
	pthread_cond_destroy(&ctx->cond);
	free(ctx->btrs);
	free(ctx);
}
