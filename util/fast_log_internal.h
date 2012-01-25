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

#ifndef REDFISH_UTIL_FAST_LOG_INTERNAL_DOT_H
#define REDFISH_UTIL_FAST_LOG_INTERNAL_DOT_H

#include "util/bitfield.h"
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
	/** Which log types are stored */
	BITFIELD_DECL(stored, FAST_LOG_TYPE_MAX);
	/** Storage callback */
	fast_log_storage_fn_t store;
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
	/** Pointer to an array of MAX_FAST_LOG_TYPES function pointers.
	 * These can't change after initialization. */
	const fast_log_dumper_fn_t *dumpers;
	/** Which log types are stored */
	BITFIELD_DECL(stored, FAST_LOG_TYPE_MAX);
	/** Storage callback */
	fast_log_storage_fn_t store;
};

/** Allocate a fast_log buffer.
 *
 * @param fbname	The name of the fast_log buffer to create. If it is
 *			longer than fast_log_BUF_NAME_MAX, it will be truncated
 *
 * @return		The fast_log on success, or an error pointer on failure
 */
extern struct fast_log_buf* fast_log_alloc(const char *fbname);

#endif
