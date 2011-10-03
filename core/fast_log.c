/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/compiler.h"
#include "util/safe_io.h"
#include "core/fast_log.h"

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

/** Maximum number of fast_log buffers that can exist. */
#define MAX_FASTLOG_BUFS 128

#define FASTLOG_BUF_SZ (4096 * 4)

#define FASTLOG_MAX_OFF (FASTLOG_BUF_SZ / sizeof(struct fast_log_entry))

struct fast_log_buf
{
	/** If 1, this is registered in g_fast_log_bufs */
	int registered;
	/** Name of this fast_log buffer */
	char name[FAST_LOG_BUF_NAME_MAX];
	/** Pointer to an mmap'ed buffer of size FASTLOG_BUF_SZ */
	char *buf;
	/** Current offset within the buffer */
	uint32_t off;
};

/** The dumper functions that we recognize. Indexed by type. */
static const fast_log_dumper_fn_t *g_dumpers;

/** A list of all of the fast_log buffer objects that have been created. */
static struct fast_log_buf *g_fast_log_bufs[MAX_FASTLOG_BUFS];

/** Lock that protects the fast_log list */
static pthread_spinlock_t g_dumpers_lock;

int fast_log_init(const fast_log_dumper_fn_t *dumpers)
{
	int ret;
	ret = pthread_spin_init(&g_dumpers_lock, 0);
	if (ret)
		return ret;
	g_dumpers = dumpers;
	memset(g_fast_log_bufs, 0, sizeof(g_fast_log_bufs));
	return 0;
}

static void fast_log_free(struct fast_log_buf* fb)
{
	munmap(fb->buf, FASTLOG_BUF_SZ);
	free(fb);
}

struct fast_log_buf* fast_log_create(const char *name)
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

int fast_log_register_buffer(struct fast_log_buf *fb)
{
	int i;

	/* Insert into our global list of fast_logs. */
	pthread_spin_lock(&g_dumpers_lock);
	for (i = 0; i < MAX_FASTLOG_BUFS; ++i) {
		if (g_fast_log_bufs[i] == NULL)
			break;
	}
	if (i == MAX_FASTLOG_BUFS) {
		pthread_spin_unlock(&g_dumpers_lock);
		return -ENOBUFS;
	}
	fb->registered = 1;
	g_fast_log_bufs[i] = fb;
	pthread_spin_unlock(&g_dumpers_lock);
	return 0;
}

void fast_log_destroy(struct fast_log_buf* fb)
{
	if (fb->registered) {
		int i;

		/* Remove from our global list of fast_logs. */
		pthread_spin_lock(&g_dumpers_lock);
		for (i = 0; i < MAX_FASTLOG_BUFS; ++i) {
			if (g_fast_log_bufs[i] == fb) {
				g_fast_log_bufs[i] = NULL;
				break;
			}
		}
		pthread_spin_unlock(&g_dumpers_lock);
	}
	fast_log_free(fb);
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
static void fast_log_copy(struct fast_log_buf *dst,
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
static int fast_log_dump_impl(const struct fast_log_buf* scratch, int fd)
{
	const char dump_header[] = "*** FASTLOG ";
	struct fast_log_entry *buf = (struct fast_log_entry*)scratch->buf;
	uint32_t off, start_off;
	int res;

	res = safe_write(fd, dump_header, sizeof(dump_header) - 1);
	res = safe_write(fd, scratch->name, signal_safe_strlen(scratch->name));
	res = safe_write(fd, "\n", 1);

	off = start_off = scratch->off;
	do {
		uint16_t type;
		struct fast_log_entry *fe = buf + off;
		off++;
		if (off == FASTLOG_MAX_OFF) {
			off = 0;
		}
		memcpy(&type, &fe->type, sizeof(uint16_t));
		if (type < FAST_LOG_TYPE_MAX) {
			fast_log_dumper_fn_t fn = g_dumpers[type];
			if (fn) {
				int ret = fn(fe, fd);
				if (ret)
					return ret;
			}
		}
	} while (off != start_off);

	return 0;
}

/* Please remember that this function has to be signal-safe. */
int fast_log_dump(const struct fast_log_buf* fb,
                struct fast_log_buf* scratch, int fd)
{
	fast_log_copy(scratch, fb);
	return fast_log_dump_impl(scratch, fd);
}

/* Please remember that this function has to be signal-safe. */
int fast_log_dump_all(struct fast_log_buf* scratch, int fd)
{
	int ret, i = 0;
	if (g_dumpers == NULL)
		return 0;
	pthread_spin_lock(&g_dumpers_lock);
	while (1) {
		if (i == MAX_FASTLOG_BUFS) {
			pthread_spin_unlock(&g_dumpers_lock);
			return 0;
		}
		if (g_fast_log_bufs[i] == NULL) {
			i++;
			continue;
		}
		fast_log_copy(scratch, g_fast_log_bufs[i]);
		pthread_spin_unlock(&g_dumpers_lock);
		i++;
		ret = fast_log_dump_impl(scratch, fd);
		if (ret)
			return ret;
	}
}
