/*
 * Copyright 2011-2012 the RedFish authors
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

#include "util/compiler.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_internal.h"
#include "util/fast_log_mgr.h"
#include "util/fast_log_types.h"
#include "util/safe_io.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define FASTLOG_BUF_SZ (4096 * 4)

#define FASTLOG_MAX_OFF (FASTLOG_BUF_SZ / sizeof(struct fast_log_entry))

/** Intervals at which we check the fast log manager's message storage settings.
 * It would be simple to check the manager's storage settings before every
 * message, but that would eliminate most of the speed advantage of fast log.
 * So we check every FASTLOG_MAX_OFF / 4 logs.
 * */
#define FASTLOG_CHECKED_OFF (FASTLOG_MAX_OFF / 4)

struct fast_log_buf* fast_log_alloc(const char *name)
{
	struct fast_log_buf *fb;

	fb = calloc(1, sizeof(struct fast_log_buf));
	if (!fb)
		return NULL;
	snprintf(fb->name, FAST_LOG_BUF_NAME_MAX, "%s", name);
	fb->buf = mmap(NULL, FASTLOG_BUF_SZ, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (fb->buf == MAP_FAILED) {
		free(fb);
		return NULL;
	}
	return fb;
}

struct fast_log_buf* fast_log_create(struct fast_log_mgr *mgr, const char *name)
{
	struct fast_log_buf *fb = fast_log_alloc(name);
	if (IS_ERR(fb))
		return fb;
	fb->mgr = mgr;
	fast_log_mgr_cp_storage_settings(mgr,
			fb->stored, &fb->store);
	fast_log_mgr_register_buffer(mgr, fb);
	return fb;
}

void fast_log_set_name(struct fast_log_buf *fb, const char *name)
{
	snprintf(fb->name, FAST_LOG_BUF_NAME_MAX, "%s", name);
}

void fast_log_free(struct fast_log_buf* fb)
{
	if (fb->mgr) {
		fast_log_mgr_unregister_buffer(fb->mgr, fb);
	}
	munmap(fb->buf, FASTLOG_BUF_SZ);
	free(fb);
}

void fast_log(struct fast_log_buf* fb, void *entry)
{
	struct fast_log_entry *fe = (struct fast_log_entry*)entry;
	struct fast_log_entry *buf = (struct fast_log_entry*)fb->buf;

	if (fe->type >= FAST_LOG_TYPE_MAX)
		return;
	if (BITFIELD_TEST(fb->stored, fe->type)) {
		char buf[FAST_LOG_PRETTY_PRINTED_MAX] = { 0 };
		fb->mgr->dumpers[fe->type](entry, buf);
		fb->store(buf);
	}
	memcpy(buf + fb->off, fe, sizeof(struct fast_log_entry));
	fb->off++;
	if (fb->off % FASTLOG_CHECKED_OFF == 0) {
		fast_log_mgr_cp_storage_settings(fb->mgr,
				fb->stored, &fb->store);
	}
	if (fb->off == FASTLOG_MAX_OFF) {
		fb->off = 0;
	}
}

/* Please remember that this function has to be signal-safe. */
void fast_log_copy(struct fast_log_buf *dst,
			const struct fast_log_buf *src)
{
	memcpy(dst->name, src->name, FAST_LOG_BUF_NAME_MAX);
	dst->off = src->off;
	memcpy(dst->buf, src->buf, FASTLOG_BUF_SZ);
}

/* Yes, strlen is not defined to be signal-safe, so I had to write my own. Dumb.
 */
static int signal_safe_strlen(const char *str)
{
	int i = 0;
	while (1) {
		if (str[i] == '\0')
			return i;
		i++;
	}
}

/* Please remember that this function has to be signal-safe. */
int fast_log_dump(const struct fast_log_buf* fb,
		const fast_log_dumper_fn_t *dumpers, int fd)
{
	const char dump_header[] = "*** FASTLOG ";
	struct fast_log_entry *buf = (struct fast_log_entry*)fb->buf;
	uint32_t off, start_off;
	int res;

	res = safe_write(fd, dump_header, sizeof(dump_header) - 1);
	res = safe_write(fd, fb->name, signal_safe_strlen(fb->name));
	res = safe_write(fd, "\n", 1);

	off = start_off = fb->off;
	do {
		uint16_t type;
		struct fast_log_entry *fe = buf + off;
		off++;
		if (off == FASTLOG_MAX_OFF) {
			off = 0;
		}
		memcpy(&type, &fe->type, sizeof(uint16_t));
		if (type < FAST_LOG_TYPE_MAX) {
			fast_log_dumper_fn_t fn = dumpers[type];
			if (fn) {
				char tmp[FAST_LOG_PRETTY_PRINTED_MAX] = { 0 };
				fn(fe, tmp);
				res = safe_write(fd, tmp,
					signal_safe_strlen(tmp));
				if (res)
					return res;
			}
		}
	} while (off != start_off);

	return 0;
}
