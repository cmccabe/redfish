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

#include "client/fishc.h"
#include "util/compiler.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* "Convenience" functions that are the same in any client implementation. */
void redfish_disconnect_and_release(struct redfish_client *cli)
{
	redfish_disconnect(cli);
	redfish_release_client(cli);
}

int redfish_close_and_free(struct redfish_file *ofe)
{
	int ret;

	ret = redfish_close(ofe);
	redfish_free_file(ofe);
	return ret;
}

void redfish_free_path_status(struct redfish_stat* osa)
{
	free(osa->owner);
	free(osa->group);
}

void redfish_free_dir_entries(struct redfish_dir_entry* odas, int noda)
{
	int i;

	for (i = 0; i < noda; ++i) {
		struct redfish_dir_entry *oda = odas + i;
		free(oda->name);
		redfish_free_path_status(&oda->stat);
	}
	free(odas);
}

void redfish_free_block_locs(struct redfish_block_loc **blcs, int nblc)
{
	struct redfish_block_loc *blc;
	int i, j;

	for (i = 0; i < nblc; ++i) {
		blc = blcs[i];
		if (!blc)
			continue;
		for (j = 0; j < blc->nhosts; ++j) {
			free(blc->hosts[j].hostname);
		}
		free(blc);
	}
	free(blcs);
}

void redfish_log_to_dev_null(POSSIBLY_UNUSED(void *log_ctx),
		POSSIBLY_UNUSED(const char *msg))
{
	/* Ignore the log message. */
}

void redfish_log_to_stderr(POSSIBLY_UNUSED(void *log_ctx), const char *str)
{
	fputs(str, stderr);
}

void client_log(redfish_log_fn_t log_cb, void *log_ctx, const char *fmt, ...)
{
	int res;
	va_list ap, ap2;
	char stack_buf[8192];

	va_start(ap, fmt);
	va_copy(ap2, ap);
	res = vsnprintf(stack_buf, sizeof(stack_buf), fmt, ap);
	if (res + 1 >= (int)sizeof(stack_buf)) {
		char *malloc_buf = malloc(res + 1);
		if (!malloc_buf)
			return;
		vsnprintf(malloc_buf, res + 1, fmt, ap2);
		log_cb(log_ctx, malloc_buf);
		free(malloc_buf);
	}
	else {
		log_cb(log_ctx, stack_buf);
	}
	va_end(ap2);
	va_end(ap);
}
