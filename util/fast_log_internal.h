/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_FAST_LOG_INTERNAL_DOT_H
#define REDFISH_UTIL_FAST_LOG_INTERNAL_DOT_H

#include "util/fast_log.h"
#include "util/fast_log_mgr.h"
#include "util/queue.h"

#include <pthread.h> /* for pthread_spinlock_t */
#include <stdint.h> /* for uint32_t, etc. */

struct fast_log_buf
{
	/** The fast log buffer manager for this buffer, or NULL if this buffer
	 * is unmanaged. */
	struct fast_log_mgr *mgr;
	/** Implicit doubly-linked list of fast log buffers */
	LIST_ENTRY(fast_log_buf) entry;
	/** Name of this fast_log buffer */
	char name[FAST_LOG_BUF_NAME_MAX];
	/** Pointer to an mmap'ed buffer of size FASTLOG_BUF_SZ */
	char *buf;
	/** Current offset within the buffer */
	uint32_t off;
};

LIST_HEAD(fast_log_buf_list, fast_log_buf);

struct fast_log_mgr
{
	/** Normally: the list of all buffers
	 * During a "dump all" operation: the buffers we haven't dumped yet */
	struct fast_log_buf_list buf_head;
	/** Normally: empty
	 * During a "dump all" operation: the buffers we already dumped */
	struct fast_log_buf_list dumped_head;
	/** Spinlock protecting the fast log buffer list */
	pthread_spinlock_t lock;
	/** A fast log buffer used as scratch during the dumping process */
	struct fast_log_buf *scratch;
	/** Pointer to an array of MAX_FAST_LOG_TYPES function pointers */
	const fast_log_dumper_fn_t *dumpers;
};

#endif
