/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/process_ctx.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "util/compiler.h"
#include "util/macro.h"
#include "util/test.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#define MSGR_UNIT_PORT 9095

enum {
	MMM_TEST1 = 9000,
	MMM_TEST2,
};

PACKED_ALIGNED(4,
struct mmm_test1 {
	struct msg base;
	uint32_t i;
});

PACKED_ALIGNED(4,
struct mmm_test2 {
	struct msg base;
	uint32_t i;
});

uint32_t g_localhost = INADDR_NONE;

struct foo_tran {
	struct mtran base;
	uint32_t i;
};

static sem_t g_msgr_test_simple_send_sem;

void foo_cb(struct mconn *conn, struct mtran *tr, struct msg *m)
{
	struct mmm_test2 *mm;
	struct foo_tran *ft = (struct foo_tran*)tr;
	uint16_t ty;
	uint32_t i;

	fprintf(stderr, "invoking tr %p\n", tr);
	if (!m) {
		mtran_recv_next(conn, tr);
		return;
	}
	ty = be16toh(m->ty);
	if (ty != MMM_TEST2) {
		fprintf(stderr, "foo_cb: expected type %d, got type %d\n",
			MMM_TEST2, ty);
		goto done;
	}
	mm = (struct mmm_test2*)m;
	i = be32toh(mm->i);
	if (i != (ft->i + 1)) {
		fprintf(stderr, "foo_cb: expected i=%d, got i=%d\n",
			ft->i + 1, i);
		goto done;
	}
	fprintf(stderr, "freeing tr %p\n", tr);
	sem_post(&g_msgr_test_simple_send_sem);
	mtran_free(tr);
done:
	free(m);
}

struct bar_tran {
	struct mtran base;
};

void bar_cb(struct mconn *conn, struct mtran *tr, struct msg *msg)
{
	struct mmm_test1 *m;
	struct mmm_test2 *mout;
	uint32_t i;
	uint16_t ty;
	if (!msg) {
		mtran_free(tr);
		return;
	}
	m = (struct mmm_test1*)msg;
	ty = be16toh(m->base.ty);
	if (ty != MMM_TEST1) {
		fprintf(stderr, "bar_cb: expected type %d, got type %d\n",
			MMM_TEST1, ty);
		goto done;
	}
	fprintf(stderr, "%02x %02x %02x %02x\n", m->base.data[0],
		m->base.data[1], m->base.data[2], m->base.data[3]);
	i = be32toh(m->i);
	mout = calloc_msg(MMM_TEST2, sizeof(struct mmm_test2));
	if (!mout) {
		fprintf(stderr, "bar_cb: oom\n");
		goto done;
	}
	mout->i = htobe32(i + 1);
	mtran_send_next(conn, tr, (struct msg*)mout);

done:
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

	foo_msgr = msgr_init(err, err_len, 10, 10, sizeof(struct foo_tran),
			foo_cb, g_fast_log_mgr);
	if (err[0])
		goto handle_error;
	bar_msgr = msgr_init(err, err_len, 10, 10, sizeof(struct bar_tran),
			bar_cb, g_fast_log_mgr);
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
	return 0;

handle_error:
	fprintf(stderr, "msgr_test_init_shutdown: got error %s\n", err);
	return 1;
}

static int send_foo_tr(struct msgr* foo_msgr, uint32_t i)
{
	struct foo_tran *tr;
	struct mmm_test1 *mout;
	tr = mtran_alloc(foo_msgr);
	if (!tr)
		return -ENOMEM;
	mout = calloc_msg(MMM_TEST1, sizeof(struct mmm_test1));
	if (!mout)
		return -ENOMEM;
	mout->i = htobe32(i);
	tr->i = i;
	mtran_send(foo_msgr, (struct mtran*)tr, g_localhost,
		MSGR_UNIT_PORT, (struct msg*)mout);
	return 0;
}

static int msgr_test_simple_send(int num_sends)
{
	int i;
	struct msgr *foo_msgr, *bar_msgr;
	char err[512] = { 0 };
	size_t err_len = sizeof(err);

	EXPECT_ZERO(sem_init(&g_msgr_test_simple_send_sem, 0, 0));

	foo_msgr = msgr_init(err, err_len, 10, 10, sizeof(struct foo_tran),
				foo_cb, g_fast_log_mgr);
	if (err[0])
		goto handle_error;
	bar_msgr = msgr_init(err, err_len, 10, 10, sizeof(struct bar_tran),
				bar_cb, g_fast_log_mgr);
	if (err[0])
		goto handle_error;
	msgr_listen(bar_msgr, MSGR_UNIT_PORT, err, err_len);
	if (err[0])
		goto handle_error;
	msgr_start(foo_msgr, err, err_len);
	if (err[0])
		goto handle_error;
	msgr_start(bar_msgr, err, err_len);
	if (err[0])
		goto handle_error;
	for (i = 0; i < num_sends; ++i) {
		EXPECT_ZERO(send_foo_tr(foo_msgr, i + 1));
	}
	for (i = 0; i < num_sends; ++i) {
		sem_wait(&g_msgr_test_simple_send_sem);
	}
	EXPECT_ZERO(sem_destroy(&g_msgr_test_simple_send_sem));

	msgr_shutdown(foo_msgr);
	msgr_shutdown(bar_msgr);
	return 0;

handle_error:
	fprintf(stderr, "msgr_test_simple_send: got error %s\n", err);
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
	process_ctx_shutdown();

	return EXIT_SUCCESS;
}
