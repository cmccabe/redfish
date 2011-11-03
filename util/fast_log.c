/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/compiler.h"
#include "util/fast_log.h"
#include "util/fast_log_internal.h"
#include "util/fast_log_mgr.h"
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

void fast_log_free(struct fast_log_buf* fb)
{
	munmap(fb->buf, FASTLOG_BUF_SZ);
	free(fb);
}

void fast_log(struct fast_log_buf* fb, void *fe)
{
	struct fast_log_entry *buf = (struct fast_log_entry*)fb->buf;
	memcpy(buf + fb->off, fe, sizeof(struct fast_log_entry));
	fb->off++;
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
				char tmp[FAST_LOG_ENTRY_MAX] = { 0 };
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
