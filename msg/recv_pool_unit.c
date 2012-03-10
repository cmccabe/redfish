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

#include "core/process_ctx.h"
#include "msg/recv_pool.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "util/compiler.h"
#include "util/fast_log.h"
#include "util/macro.h"
#include "util/packed.h"
#include "util/test.h"

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSGR_UNIT_PORT 9096
#define RECV_POOL_UNIT_RT_MAX 32

enum {
	MMM_TEST40 = 9040,
};

PACKED(
struct mmm_test40 {
	struct msg base;
	uint32_t q;
});

static uint32_t g_localhost;

static int recv_pool_test_init_shutdown(void)
{
	struct recv_pool *rpool;

	rpool = recv_pool_init("my_tpool");
	EXPECT_NOT_ERRPTR(rpool);
	recv_pool_join(rpool);
	recv_pool_free(rpool);
	return 0;
}

static int send_foo_tr(struct msgr* msgr, msgr_cb_t cb, uint32_t q)
{
	struct mtran *tr;
	struct mmm_test40 *mout;
	tr = mtran_alloc(msgr);
	if (!tr)
		return -ENOMEM;
	mout = calloc_msg(MMM_TEST40, sizeof(struct mmm_test40));
	if (!mout) {
		mtran_free(tr);
		return -ENOMEM;
	}
	pack_to_be32(&mout->q, q);
	tr->ip = g_localhost;
	tr->port = MSGR_UNIT_PORT;
	mtran_send(msgr, tr, cb, (void*)(uintptr_t)q, (struct msg*)mout, 60);
	return 0;
}

static void foo_cb(POSSIBLY_UNUSED(struct mconn *conn), struct mtran *tr)
{
	if (tr->state != MTRAN_STATE_SENT)
		abort();
	if (tr->m)
		abort();
	mtran_free(tr);
}

static sem_t g_handler_run_sem;
static pthread_cond_t g_full_set_cond;
static pthread_mutex_t g_full_set_lock;
static uint32_t g_current_iter_lowest;

static int recv_pool_test_handler(POSSIBLY_UNUSED(struct recv_pool_thread *rt),
		struct mtran *tr)
{
	struct mmm_test40 *m = (struct mmm_test40 *)tr->m;
	uint32_t q;
	uint16_t ty;

	ty = unpack_from_be16(&m->base.ty);
	if (ty != MMM_TEST40)
		abort();
	q = unpack_from_be32(&m->q);
	pthread_mutex_lock(&g_full_set_lock);
	if (q < g_current_iter_lowest) {
		pthread_mutex_unlock(&g_full_set_lock);
		return -EINVAL;
	}
	pthread_mutex_unlock(&g_full_set_lock);
	sem_post(&g_handler_run_sem);
	pthread_mutex_lock(&g_full_set_lock);
	while (1) {
		if (q < g_current_iter_lowest) {
			pthread_mutex_unlock(&g_full_set_lock);
			return 0;
		}
		pthread_cond_wait(&g_full_set_cond, &g_full_set_lock);
	}
}

static int recv_pool_test_recv(int nthreads, int niter)
{
	int i, iter;
	struct msgr *foo_msgr, *bar_msgr;
	struct recv_pool *rpool;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	EXPECT_ZERO(sem_init(&g_handler_run_sem, 0, 0));
	EXPECT_ZERO(pthread_cond_init(&g_full_set_cond, NULL));
	EXPECT_ZERO(pthread_mutex_init(&g_full_set_lock, NULL));
	g_current_iter_lowest = 0;
	rpool = recv_pool_init("my_tpool");
	EXPECT_NOT_ERRPTR(rpool);
	foo_msgr = msgr_init(err, err_len, 10, 10,
			360, g_fast_log_mgr, "foo_msgr");
	EXPECT_ZERO(err[0]);
	bar_msgr = msgr_init(err, err_len, 10, 10,
			360, g_fast_log_mgr, "bar_msgr");
	EXPECT_ZERO(err[0]);
	recv_pool_msgr_listen(rpool, bar_msgr,
			MSGR_UNIT_PORT, err, sizeof(err));
	EXPECT_ZERO(err[0]);
	msgr_start(foo_msgr, err, err_len);
	EXPECT_ZERO(err[0]);
	msgr_start(bar_msgr, err, err_len);
	EXPECT_ZERO(err[0]);

	for (i = 0; i < nthreads; ++i) {
		EXPECT_ZERO(recv_pool_thread_create(rpool,
			g_fast_log_mgr, recv_pool_test_handler,
			(void*)(uintptr_t)i));
	}
	for (iter = 0; iter < niter; ++iter) {
		for (i = 0; i < nthreads; ++i) {
			EXPECT_ZERO(send_foo_tr(foo_msgr, foo_cb,
				(nthreads * iter) +  i));
		}
		for (i = 0; i < nthreads; ++i) {
			sem_wait(&g_handler_run_sem);
		}
		pthread_mutex_lock(&g_full_set_lock);
		g_current_iter_lowest = nthreads * (iter + 1);
		pthread_cond_broadcast(&g_full_set_cond);
		pthread_mutex_unlock(&g_full_set_lock);
	}
	recv_pool_join(rpool);
	recv_pool_free(rpool);

	msgr_shutdown(foo_msgr);
	msgr_shutdown(bar_msgr);
	msgr_free(foo_msgr);
	msgr_free(bar_msgr);
	sem_destroy(&g_handler_run_sem);
	pthread_cond_destroy(&g_full_set_cond);
	pthread_mutex_destroy(&g_full_set_lock);
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	EXPECT_ZERO(utility_ctx_init(argv[0]));
	EXPECT_ZERO(get_localhost_ipv4(&g_localhost));
	EXPECT_ZERO(recv_pool_test_init_shutdown());
	EXPECT_ZERO(recv_pool_test_recv(1, 1));
	EXPECT_ZERO(recv_pool_test_recv(3, 1));
	EXPECT_ZERO(recv_pool_test_recv(10, 5));
	process_ctx_shutdown();

	return EXIT_SUCCESS;
}
