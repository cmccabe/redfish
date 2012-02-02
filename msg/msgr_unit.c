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

#include "core/process_ctx.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "util/compiler.h"
#include "util/macro.h"
#include "util/packed.h"
#include "util/test.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSGR_UNIT_PORT 9095

enum {
	MMM_TEST1 = 9000,
	MMM_TEST2,
};

PACKED(
struct mmm_test1 {
	struct msg base;
	uint32_t i;
});

PACKED(
struct mmm_test2 {
	struct msg base;
	uint32_t i;
});

uint32_t g_localhost = INADDR_NONE;

static sem_t g_msgr_test_simple_send_sem;

static void foo_cb(struct mconn *conn, struct mtran *tr)
{
	struct mmm_test2 *mm;
	uint16_t ty;
	uint32_t i, tr_i;

	//fprintf(stderr, "invoking tr %p\n", tr);
	if (tr->state == MTRAN_STATE_SENT) {
		if (tr->m && IS_ERR(tr->m)) {
			fprintf(stderr, "foo_cb: send error %d\n",
				PTR_ERR(tr->m));
			abort();
		}
		mtran_recv_next(conn, tr);
		return;
	}
	else if (tr->state != MTRAN_STATE_RECV) {
		fprintf(stderr, "foo_cb: mtran in unexpected state %s\n",
			mtran_state_to_str(tr->state));
		abort();
	}
	if (IS_ERR(tr->m)) {
		fprintf(stderr, "foo_cb: got unexpected error %d\n",
			PTR_ERR(tr->m));
		abort();
	}
	ty = unpack_from_be16(&tr->m->ty);
	if (ty != MMM_TEST2) {
		fprintf(stderr, "foo_cb: expected type %d, got type %d\n",
			MMM_TEST2, ty);
		abort();
	}
	mm = (struct mmm_test2*)tr->m;
	i = unpack_from_be32(&mm->i);
	tr_i = (uint32_t)(uintptr_t)tr->priv;
	if (i != tr_i + 1) {
		fprintf(stderr, "foo_cb: expected i=%d, got i=%d\n",
			tr_i + 1, i);
		abort();
	}
	sem_post(&g_msgr_test_simple_send_sem);
	mtran_free(tr);
}

static void bar_cb(struct mconn *conn, struct mtran *tr)
{
	struct mmm_test1 *m;
	struct mmm_test2 *mout;
	uint32_t i;
	uint16_t ty;
	if (tr->state == MTRAN_STATE_SENT) {
		if (tr->m && IS_ERR(tr->m)) {
			fprintf(stderr, "bar_cb: send error %d\n",
				PTR_ERR(tr->m));
			abort();
		}
		mtran_free(tr);
		return;
	}
	else if (tr->state != MTRAN_STATE_RECV) {
		fprintf(stderr, "bar_cb: mtran in unexpected state %s\n",
			mtran_state_to_str(tr->state));
		abort();
	}
	m = (struct mmm_test1*)tr->m;
	ty = unpack_from_be16(&m->base.ty);
	if (ty != MMM_TEST1) {
		fprintf(stderr, "bar_cb: expected type %d, got type %d\n",
			MMM_TEST1, ty);
		abort();
	}
//	fprintf(stderr, "%02x %02x %02x %02x\n", m->base.data[0],
//		m->base.data[1], m->base.data[2], m->base.data[3]);
	i = unpack_from_be32(&m->i);
	mout = calloc_msg(MMM_TEST2, sizeof(struct mmm_test2));
	if (!mout) {
		fprintf(stderr, "bar_cb: oom\n");
		abort();
	}
	pack_to_be32(&mout->i, i + 1);
	mtran_send_next(conn, tr, (struct msg*)mout);
	free(m);
}

BUILD_BUG_ON(sizeof(in_addr_t) != sizeof(uint32_t));

static int init_g_localhost(void)
{
	g_localhost = inet_addr("127.0.0.1");
	if (g_localhost == INADDR_NONE) {
		fprintf(stderr, "failed to get IP address for localhost\n");
		return 1;
	}
	g_localhost = ntohl(g_localhost);
	return 0;
}

static int msgr_test_init_shutdown(int start)
{
	struct msgr *foo_msgr, *bar_msgr;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	foo_msgr = msgr_init(err, err_len, 10, 10,
			60, 5, g_fast_log_mgr);
	if (err[0])
		goto handle_error;
	bar_msgr = msgr_init(err, err_len, 10, 10,
			60, 5, g_fast_log_mgr);
	if (err[0])
		goto handle_error;
	if (start) {
		msgr_start(foo_msgr, err, err_len);
		if (err[0])
			goto handle_error;
		msgr_start(bar_msgr, err, err_len);
		if (err[0])
			goto handle_error;
	}
	msgr_shutdown(foo_msgr);
	msgr_shutdown(bar_msgr);
	msgr_free(foo_msgr);
	msgr_free(bar_msgr);
	return 0;

handle_error:
	fprintf(stderr, "msgr_test_init_shutdown: got error %s\n", err);
	return 1;
}

static int send_foo_tr(struct msgr* msgr, msgr_cb_t cb, uint32_t i)
{
	struct mtran *tr;
	struct mmm_test1 *mout;
	tr = mtran_alloc(msgr);
	if (!tr)
		return -ENOMEM;
	mout = calloc_msg(MMM_TEST1, sizeof(struct mmm_test1));
	if (!mout) {
		mtran_free(tr);
		return -ENOMEM;
	}
	pack_to_be32(&mout->i, i);
	mtran_send(msgr, tr, g_localhost, MSGR_UNIT_PORT,
		cb, (void*)(uintptr_t)i, (struct msg*)mout);
	return 0;
}

static int msgr_test_simple_send(int num_sends)
{
	int i, res;
	struct msgr *foo_msgr, *bar_msgr;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct listen_info linfo;

	EXPECT_ZERO(sem_init(&g_msgr_test_simple_send_sem, 0, 0));

	foo_msgr = msgr_init(err, err_len, 10, 10,
				60, 5, g_fast_log_mgr);
	if (err[0])
		goto handle_error;
	bar_msgr = msgr_init(err, err_len, 10, 10,
				60, 5, g_fast_log_mgr);
	if (err[0])
		goto handle_error;
	memset(&linfo, 0, sizeof(linfo));
	linfo.cb = bar_cb;
	linfo.priv = NULL;
	linfo.port = MSGR_UNIT_PORT;
	msgr_listen(bar_msgr, &linfo, err, err_len);
	if (err[0])
		goto handle_error;
	msgr_start(foo_msgr, err, err_len);
	if (err[0])
		goto handle_error;
	msgr_start(bar_msgr, err, err_len);
	if (err[0])
		goto handle_error;
	for (i = 0; i < num_sends; ++i) {
		EXPECT_ZERO(send_foo_tr(foo_msgr, foo_cb, i + 1));
	}
	for (i = 0; i < num_sends; ++i) {
		RETRY_ON_EINTR(res, sem_wait(&g_msgr_test_simple_send_sem));
	}
	EXPECT_ZERO(sem_destroy(&g_msgr_test_simple_send_sem));

	msgr_shutdown(foo_msgr);
	msgr_shutdown(bar_msgr);
	msgr_free(foo_msgr);
	msgr_free(bar_msgr);
	return 0;

handle_error:
	fprintf(stderr, "msgr_test_simple_send: got error %s\n", err);
	return 1;
}

static sem_t g_msgr_test_baz_sem;

static void baz_cb(struct mconn *conn, struct mtran *tr)
{
	uint32_t ex;

	if (tr->state == MTRAN_STATE_SENT) {
		if (!tr->m) {
			mtran_recv_next(conn, tr);
			return;
		}
		ex = (uint32_t)(uintptr_t)tr->priv;
		if (PTR_ERR(tr->m) == (int)ex) {
			mtran_free(tr);
			sem_post(&g_msgr_test_baz_sem);
			return;
		}
		fprintf(stderr, "baz_cb: got unexpected send error %d.\n",
			PTR_ERR(tr->m));
		abort();
	}
	else if (tr->state != MTRAN_STATE_RECV) {
		fprintf(stderr, "baz_cb: mtran in unexpected state %s\n",
			mtran_state_to_str(tr->state));
		abort();
	}
	if (IS_ERR(tr->m)) {
		ex = (uint32_t)(uintptr_t)tr->priv;
		if (PTR_ERR(tr->m) == (int)ex) {
			mtran_free(tr);
			sem_post(&g_msgr_test_baz_sem);
			return;
		}
		fprintf(stderr, "baz_cb: MTRAN_STATE_RECV error %d.  We"
			"expected error %d instead.\n", PTR_ERR(tr->m), ex);
		abort();
	}
	/* We don't send a response here.  That's what allows us to test
	 * timeouts, etc. */
	mtran_free(tr);
}

static int msgr_test_conn_timeout(void)
{
	int res;
	struct msgr *baz1_msgr, *baz2_msgr;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct listen_info linfo;

	EXPECT_ZERO(sem_init(&g_msgr_test_baz_sem, 0, 0));

	baz1_msgr = msgr_init(err, err_len, 10, 10,
				1, 1, g_fast_log_mgr);
	if (err[0])
		goto handle_error;
	baz2_msgr = msgr_init(err, err_len, 10, 10,
				1, 1, g_fast_log_mgr);
	if (err[0])
		goto handle_error;
	memset(&linfo, 0, sizeof(linfo));
	linfo.cb = baz_cb;
	linfo.priv = NULL;
	linfo.port = MSGR_UNIT_PORT;
	msgr_listen(baz2_msgr, &linfo, err, err_len);
	if (err[0])
		goto handle_error;
	msgr_start(baz1_msgr, err, err_len);
	if (err[0])
		goto handle_error;
	msgr_start(baz2_msgr, err, err_len);
	if (err[0])
		goto handle_error;
	EXPECT_ZERO(send_foo_tr(baz1_msgr, baz_cb, ETIMEDOUT));
	RETRY_ON_EINTR(res, sem_wait(&g_msgr_test_baz_sem));
	EXPECT_ZERO(sem_destroy(&g_msgr_test_baz_sem));

	msgr_shutdown(baz1_msgr);
	msgr_shutdown(baz2_msgr);
	msgr_free(baz1_msgr);
	msgr_free(baz2_msgr);
	return 0;

handle_error:
	fprintf(stderr, "msgr_test_conn_timeout: got error %s\n", err);
	return 1;
}

static int msgr_test_conn_cancel(void)
{
	int res;
	struct msgr *baz1_msgr, *baz2_msgr;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct listen_info linfo;

	EXPECT_ZERO(sem_init(&g_msgr_test_baz_sem, 0, 0));

	baz1_msgr = msgr_init(err, err_len, 10, 10,
				1, 1, g_fast_log_mgr);
	if (err[0])
		goto handle_error;
	baz2_msgr = msgr_init(err, err_len, 10, 10,
				1, 1, g_fast_log_mgr);
	if (err[0])
		goto handle_error;
	memset(&linfo, 0, sizeof(linfo));
	linfo.cb = baz_cb;
	linfo.priv = NULL;
	linfo.port = MSGR_UNIT_PORT;
	msgr_listen(baz2_msgr, &linfo, err, err_len);
	if (err[0])
		goto handle_error;
	msgr_start(baz1_msgr, err, err_len);
	if (err[0])
		goto handle_error;
	msgr_start(baz2_msgr, err, err_len);
	if (err[0])
		goto handle_error;
	EXPECT_ZERO(send_foo_tr(baz1_msgr, baz_cb, ECANCELED));
	msgr_shutdown(baz1_msgr);
	EXPECT_ZERO(send_foo_tr(baz1_msgr, baz_cb, ECANCELED));
	RETRY_ON_EINTR(res, sem_wait(&g_msgr_test_baz_sem));
	RETRY_ON_EINTR(res, sem_wait(&g_msgr_test_baz_sem));
	EXPECT_ZERO(sem_destroy(&g_msgr_test_baz_sem));

	msgr_shutdown(baz2_msgr);
	msgr_free(baz1_msgr);
	msgr_free(baz2_msgr);
	return 0;

handle_error:
	fprintf(stderr, "msgr_test_conn_cancel: got error %s\n", err);
	return 1;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	EXPECT_ZERO(utility_ctx_init(argv[0]));
	EXPECT_ZERO(init_g_localhost());
	EXPECT_ZERO(msgr_test_init_shutdown(0));
	EXPECT_ZERO(msgr_test_init_shutdown(1));
	EXPECT_ZERO(msgr_test_simple_send(1));
	EXPECT_ZERO(msgr_test_simple_send(100));
	EXPECT_ZERO(msgr_test_conn_timeout());
	EXPECT_ZERO(msgr_test_conn_cancel());
	process_ctx_shutdown();

	return EXIT_SUCCESS;
}
