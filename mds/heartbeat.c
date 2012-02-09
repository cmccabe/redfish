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

#include "common/cluster_map.h"
#include "common/config/mdsc.h"
#include "common/config/unitaryc.h"
#include "core/alarm.h"
#include "core/glitch_log.h"
#include "core/process_ctx.h"
#include "jorm/jorm_const.h"
#include "mds/delegation.h"
#include "mds/dmap.h"
#include "mds/dslots.h"
#include "mds/limits.h"
#include "mds/net.h"
#include "msg/bsend.h"
#include "msg/mds.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "msg/recv_pool.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_types.h"
#include "util/packed.h"
#include "util/string.h"
#include "util/thread.h"
#include "util/time.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MDS_HB_SEND_IVAL 5

#define MDS_SUICIDE_TIMEOUT 40

int mds_send_hb_thread(struct redfish_thread *rt)
{
	struct mtran *tr;
	struct bsend *ctx;
	int succ, i, num_mds, ret, first, use_timer_a, num_sent;
	struct mmm_mds_heartbeat *m;
	time_t cur_time, until;
	timer_t timer_a, timer_b;
	struct daemon_info *di;
	char buf[128];

	/* We want a pretty short timeout here.  If we fail to send a heartbeat
	 * to an MDS, hopefully it will receive the next heartbeat that we send.
	 * We don't want to block for a long time because that would cause us to
	 * fail to deliver heartbeats to other MDSes.
	 */
	ctx = bsend_init(rt->fb, RF_MAX_MDS);
	if (IS_ERR(ctx)) {
		glitch_log("mds_send_hb_thread: failed to allocate "
			"an RPC context for the heartbeat thread: "
			"error %d\n", PTR_ERR(ctx));
		abort();
	}
	until = mt_time() + MDS_HB_SEND_IVAL;
	first = 1;
	use_timer_a = 1;
	while (1) {
		mt_sleep_until(until);
		cur_time = mt_time();
		until = cur_time + MDS_HB_SEND_IVAL;
		ret = mt_set_alarm(cur_time + MDS_SUICIDE_TIMEOUT,
				"mds_send_hb_thread timeout",
				use_timer_a ? &timer_a : &timer_b);
		if (ret) {
			glitch_log("mds_send_hb_thread: failed to set "
				"timer: error %d\n", ret);
			abort();
		}
		if (!first)
			mt_deactivate_alarm(use_timer_a ? &timer_b : &timer_a);
		first = 0;
		use_timer_a = !use_timer_a;
		pthread_mutex_lock(&g_cmap_lock);
		num_mds = g_cmap->num_mds;
		for (i = 0; i < num_mds; ++i) {
			if (i == g_mid)
				continue;
			di = &g_cmap->minfo[i];
			if (!di->in)
				continue;
			m = calloc_msg(MMM_MDS_HEARTBEAT,
				sizeof(struct mmm_mds_heartbeat));
			if (!m) {
				glitch_log("mds_send_hb_thread: failed to "
					"allocate memory for heartbeat "
					"message.\n");
				abort();
			}
			pack_to_be16(&m->mid, g_mid);
			bsend_add(ctx, g_mds_msgr, 0, (struct msg*)m, di->ip, di->port);
		}
		pthread_mutex_unlock(&g_cmap_lock);

		num_sent = bsend_join(ctx);
		if (num_sent < 0)
			return num_sent;
		succ = 0;
		for (i = 0; i < num_sent; ++i) {
			tr = bsend_get_mtran(ctx, i);
			if (tr->m && IS_ERR(tr->m)) {
				mtran_ep_to_str(tr, buf, sizeof(buf));
				glitch_log("mds_send_hb_thread: failed to "
					"send heartbeat to %s: error %d\n",
					buf, PTR_ERR(tr->m));
			}
			else
				++succ;
		}
		glitch_log("mds_send_hb_thread: successfully sent %d "
			   "heartbeats out of %d\n", succ, num_sent);
		bsend_reset(ctx);
	}
}
