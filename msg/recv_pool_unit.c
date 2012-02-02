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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSGR_UNIT_PORT 9096

enum {
	MMM_TEST40 = 9040,
	MMM_TEST41,
};

PACKED(
struct mmm_test40 {
	struct msg base;
	uint32_t q;
});

PACKED(
struct mmm_test41 {
	struct msg base;
	uint32_t u;
});

uint32_t g_localhost = INADDR_NONE;

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

static int recv_pool_test_init_shutdown(struct fast_log_buf *fb)
{
	struct recv_pool *rpool;

	rpool = recv_pool_init(fb);
	EXPECT_NOT_ERRPTR(rpool);
	recv_pool_join(rpool);
	recv_pool_free(rpool);
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	struct fast_log_buf *fb;
	
	EXPECT_ZERO(utility_ctx_init(argv[0]));
	fb = fast_log_create(g_fast_log_mgr, "recv_pool_unit_main");
	EXPECT_NOT_ERRPTR(fb);
	EXPECT_ZERO(init_g_localhost());
	EXPECT_ZERO(recv_pool_test_init_shutdown(fb));
	process_ctx_shutdown();

	return EXIT_SUCCESS;
}
