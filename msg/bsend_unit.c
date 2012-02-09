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
#include "msg/bsend.h"
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSGR_UNIT_PORT 9096

enum {
	MMM_TEST30 = 9030,
	MMM_TEST31,
};

PACKED(
struct mmm_test30 {
	struct msg base;
	uint32_t x;
	uint32_t y;
});

PACKED(
struct mmm_test31 {
	struct msg base;
	uint32_t z;
});

static uint32_t g_localhost;

static void bsend_test_init_shutdown(struct fast_log_buf *fb)
{
	struct bsend *ctx;

	ctx = bsend_init(fb, 10);
	bsend_cancel(ctx);
	bsend_free(ctx);
}

static void bsend_test_cb(struct mconn *conn, struct mtran *tr)
{
	struct mmm_test30 *m;
	struct mmm_test31 *mout;
	uint32_t x, y;
	uint16_t ty;

	if (tr->state == MTRAN_STATE_SENT) {
		mtran_free(tr);
		return;
	}
	if (tr->state != MTRAN_STATE_RECV) {
		abort();
	}
	m = (struct mmm_test30*)tr->m;
	ty = unpack_from_be16(&m->base.ty);
	if (ty != MMM_TEST30) {
		fprintf(stderr, "bsend_test_cb: expected type %d; got "
			"type %d\n", MMM_TEST30, ty);
		abort();
	}
	mout = calloc_msg(MMM_TEST31, sizeof(struct mmm_test31));
	if (!mout) {
		fprintf(stderr, "bsend_test_cb: oom\n");
		abort();
	}
	x = unpack_from_be32(&m->x);
	y = unpack_from_be32(&m->y);
	if ((x == 0) && (y == 0)) {
		mtran_free(tr);
		return;
	}
	pack_to_be32(&mout->z, x + y);
	mtran_send_next(conn, tr, (struct msg*)mout);
	free(m);
}

static void bsend_test_cb_noresp(POSSIBLY_UNUSED(struct mconn *conn),
				struct mtran *tr)
{
	if (tr->state == MTRAN_STATE_RECV) {
		mtran_free(tr);
		return;
	}
	abort();
}

static int bsend_test30(struct bsend *ctx, struct msgr *msgr, int flags,
			int x, int y, int ex)
{
	struct mmm_test30 *m;

	m = calloc_msg(MMM_TEST30, sizeof(struct mmm_test30));
	if (!m)
		return -ENOMEM;
	pack_to_be32(&m->x, x);
	pack_to_be32(&m->y, y);
	EXPECT_EQ(bsend_add(ctx, msgr, flags, (struct msg*)m,
			g_localhost, MSGR_UNIT_PORT), ex);
	return 0;
}

static int bsend_test_setup(struct fast_log_buf *fb, struct msgr **foo_msgr,
	struct msgr **bar_msgr, struct bsend **ctx, int simult, int resp)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	struct listen_info linfo;

	*foo_msgr = msgr_init(err, err_len, 10, 10,
			60, 5, g_fast_log_mgr);
	EXPECT_ZERO(err[0]);
	*bar_msgr = msgr_init(err, err_len, 10, 10,
			60, 5, g_fast_log_mgr);
	EXPECT_ZERO(err[0]);
	memset(&linfo, 0, sizeof(linfo));
	linfo.cb = resp ? bsend_test_cb : bsend_test_cb_noresp;
	linfo.priv = NULL;
	linfo.port = MSGR_UNIT_PORT;
	msgr_listen(*bar_msgr, &linfo, err, err_len);
	EXPECT_EQ(err[0], '\0');
	msgr_start(*foo_msgr, err, err_len);
	EXPECT_EQ(err[0], '\0');
	msgr_start(*bar_msgr, err, err_len);
	EXPECT_EQ(err[0], '\0');
	*ctx = bsend_init(fb, simult);
	return 0;
}

static void bsend_test_teardown(struct msgr *foo_msgr, struct msgr *bar_msgr,
			struct bsend *ctx)
{
	bsend_cancel(ctx);
	bsend_free(ctx);
	msgr_shutdown(foo_msgr);
	msgr_shutdown(bar_msgr);
	msgr_free(foo_msgr);
	msgr_free(bar_msgr);
}

static int bsend_test_send(struct fast_log_buf *fb, int simult,
			int max_iter, int resp)
{
	int i, iter;
	struct bsend *ctx;
	struct msgr *foo_msgr, *bar_msgr;

	EXPECT_ZERO(bsend_test_setup(fb, &foo_msgr, &bar_msgr, &ctx,
				simult, resp));
	for (iter = 0; iter < max_iter; ++iter) {
		for (i = 0; i < simult; ++i) {
			uint8_t flags = resp ? BSF_RESP : 0;
			EXPECT_ZERO(bsend_test30(ctx, foo_msgr, flags,
						 i, 1, 0));
		}
		EXPECT_EQ(bsend_join(ctx), simult);
		for (i = 0; i < simult; ++i) {
			struct mtran *tr;
			struct mmm_test31 *m;
			uint16_t ty;
			uint32_t z;

			tr = bsend_get_mtran(ctx, i);
			EXPECT_NOT_ERRPTR(tr);
			if (!resp) {
				EXPECT_EQ(tr->m, NULL);
				continue;
			}
			m = (struct mmm_test31*)tr->m;
			EXPECT_NOT_ERRPTR(m);
			ty = unpack_from_be16(&m->base.ty);
			if (ty != MMM_TEST31) {
				fprintf(stderr, "bsend_test_send: expected "
					"type %d; got type %d\n",
					MMM_TEST31, ty);
				abort();
			}
			z = unpack_from_be32(&m->z);
			if ((int)z != i + 1) {
				fprintf(stderr, "bsend_test_send: expected "
					"z = %"PRId32 "for message %d; got "
					"z = %"PRId32" instead\n", i + 1, i, z);
				abort();
			}
		}
		bsend_reset(ctx);
	}

	bsend_test_teardown(foo_msgr, bar_msgr, ctx);
	return 0;
}

static int bsend_test_cancel(struct fast_log_buf *fb, int simult, int cancel)
{
	int i;
	struct bsend *ctx;
	struct msgr *foo_msgr, *bar_msgr;

	EXPECT_ZERO(bsend_test_setup(fb, &foo_msgr, &bar_msgr,
			&ctx, simult, 1));
	bsend_cancel(ctx);
	for (i = 0; i < simult; ++i) {
		int c = (simult >= cancel);
		if (simult == cancel)
			bsend_cancel(ctx);
		EXPECT_ZERO(bsend_test30(ctx, foo_msgr, BSF_RESP, i, 1,
				c ? -ECANCELED : 0));
	}
	EXPECT_EQ(bsend_join(ctx), -ECANCELED);
	bsend_reset(ctx);
	bsend_test_teardown(foo_msgr, bar_msgr, ctx);
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	struct fast_log_buf *fb;

	EXPECT_ZERO(utility_ctx_init(argv[0]));
	EXPECT_ZERO(get_localhost_ipv4(&g_localhost));
	fb = fast_log_create(g_fast_log_mgr, "main");
	bsend_test_init_shutdown(fb);
	EXPECT_ZERO(bsend_test_send(fb, 1, 1, 1));
	EXPECT_ZERO(bsend_test_send(fb, 5, 1, 1));
	EXPECT_ZERO(bsend_test_send(fb, 1, 2, 1));
	EXPECT_ZERO(bsend_test_send(fb, 10, 5, 1));
	EXPECT_ZERO(bsend_test_send(fb, 1, 1, 0));
	EXPECT_ZERO(bsend_test_send(fb, 10, 5, 0));
	EXPECT_ZERO(bsend_test_cancel(fb, 10, 0));
	EXPECT_ZERO(bsend_test_cancel(fb, 10, 5));
	fast_log_free(fb);
	process_ctx_shutdown();

	return EXIT_SUCCESS;
}
