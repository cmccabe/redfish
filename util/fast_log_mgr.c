/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_internal.h"
#include "util/fast_log_mgr.h"
#include "util/queue.h"

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

struct fast_log_mgr* fast_log_mgr_init(const fast_log_dumper_fn_t *dumpers)
{
	int ret;
	struct fast_log_mgr *mgr;

	mgr = calloc(1, sizeof(struct fast_log_mgr));
	if (!mgr)
		return ERR_PTR(ENOMEM);
	ret = pthread_spin_init(&mgr->lock, 0);
	if (ret) {
		free(mgr);
		return ERR_PTR(ret);
	}
	LIST_INIT(&mgr->buf_head);
	LIST_INIT(&mgr->dumped_head);
	mgr->dumpers = dumpers;
	BITFIELD_ZERO(mgr->stored);
	mgr->store = NULL;
	mgr->scratch = fast_log_alloc("scratch");
	if (IS_ERR(mgr->scratch)) {
		free(mgr);
		pthread_spin_destroy(&mgr->lock);
		return (struct fast_log_mgr*)mgr->scratch;
	}
	return mgr;
}

void fast_log_mgr_free(struct fast_log_mgr* mgr)
{
	pthread_spin_destroy(&mgr->lock);
	fast_log_free(mgr->scratch);
	mgr->scratch = NULL;
	while (1) {
		struct fast_log_buf *fb;
		fb = LIST_FIRST(&mgr->buf_head);

		if (!fb)
			break;
		LIST_REMOVE(fb, entry);
	}
	/* You should never free a fast log manager while it's in the middle of
	 * dumping buffers */
	if (LIST_FIRST(&mgr->dumped_head)) {
		abort();
	}
	free(mgr);
}

void fast_log_mgr_register_buffer(struct fast_log_mgr *mgr, struct fast_log_buf *fb)
{
	pthread_spin_lock(&mgr->lock);
	LIST_INSERT_HEAD(&mgr->buf_head, fb, entry);
	pthread_spin_unlock(&mgr->lock);
}

void fast_log_mgr_unregister_buffer(struct fast_log_mgr *mgr, struct fast_log_buf *fb)
{
	pthread_spin_lock(&mgr->lock);
	LIST_REMOVE(fb, entry);
	pthread_spin_unlock(&mgr->lock);
}

/* Please remember that this function has to be signal-safe. */
int fast_log_mgr_dump_all(struct fast_log_mgr *mgr, int fd)
{
	int ret = 0;

	pthread_spin_lock(&mgr->lock);
	while (1) {
		struct fast_log_buf *fb;

		fb = LIST_FIRST(&mgr->buf_head);
		if (!fb)
			break;
		LIST_REMOVE(fb, entry);
		LIST_INSERT_HEAD(&mgr->dumped_head, fb, entry);
		fast_log_copy(mgr->scratch, fb);
		pthread_spin_unlock(&mgr->lock);
		ret = fast_log_dump(mgr->scratch, mgr->dumpers, fd);
		pthread_spin_lock(&mgr->lock);
		if (ret)
			break;
	}
	LIST_SWAP(&mgr->buf_head, &mgr->dumped_head, fast_log_buf, entry);
	pthread_spin_unlock(&mgr->lock);
	return ret;
}

void fast_log_mgr_cp_storage_settings(struct fast_log_mgr *mgr,
		BITFIELD_DECL(stored, FAST_LOG_TYPE_MAX),
		fast_log_storage_fn_t *store)
{
	pthread_spin_lock(&mgr->lock);
	BITFIELD_COPY(stored, mgr->stored);
	*store = mgr->store;
	pthread_spin_unlock(&mgr->lock);
}

void fast_log_mgr_set_storage_settings(struct fast_log_mgr *mgr,
		BITFIELD_DECL(stored, FAST_LOG_TYPE_MAX),
		fast_log_storage_fn_t store)
{
	pthread_spin_lock(&mgr->lock);
	BITFIELD_COPY(mgr->stored, stored);
	mgr->store = store;
	pthread_spin_unlock(&mgr->lock);
}
