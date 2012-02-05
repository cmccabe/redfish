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

#include "mds/delegation.h"
#include "mds/dslots.h"
#include "core/glitch_log.h"
#include "util/error.h"
#include "util/queue.h"
#include "util/tree.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SLIST_HEAD(dg_list, delegation);

struct dslot {
	/** lock protecting this dslot */
	pthread_mutex_t lock;
	/** list of delegations */
	struct dg_list dg_head;
};

struct dslots {
	/** number of dslots */
	int num_dslots;
	/** dslots */
	struct dslot slot[0];
};

static int dslots_hash_dgid(const struct dslots *dslots, uint64_t dgid) PURE;
static int compare_dg_placeholder(const void *va, const void *vb) PURE;

struct dslots *dslots_init(int num_dslots)
{
	int ret, i;
	struct dslots *dslots;

	dslots = calloc(1, sizeof(struct dslots) +
			(sizeof(struct dslot) * num_dslots));
	dslots->num_dslots = num_dslots;
	if (!dslots) {
		ret = -ENOMEM;
		goto error;
	}
	for (i = 0; i < num_dslots; ++i) {
		ret = pthread_mutex_init(&dslots->slot[i].lock, NULL);
		if (ret)
			goto error_destroy_locks;
	}
	return dslots;

error_destroy_locks:
	for (; i > 0; --i) {
		pthread_mutex_destroy(&dslots->slot[i - 1].lock);
	}
error:
	return ERR_PTR(FORCE_POSITIVE(ret));
}

static int dslots_hash_dgid(const struct dslots *dslots, uint64_t dgid)
{
	return ((17 + dgid) * 13) % dslots->num_dslots;
}

struct dg_placeholder {
	void *priv;
	int slot;
};

static int compare_dg_placeholder(const void *va, const void *vb)
{
	const struct dg_placeholder *a = va;
	const struct dg_placeholder *b = vb;

	if (a->slot < b->slot)
		return -1;
	if (a->slot > b->slot)
		return 1;
	else
		return 0;
}

int dslots_add(struct dslots *dslots, struct delegation **dgs,
		int num_dgs)
{
	struct delegation *dg;
	int i = 0, prev_s = -1, s;
	struct dg_placeholder *phs;

	phs = malloc(num_dgs * sizeof(struct dg_placeholder));
	if (!phs)
		return -ENOMEM;
	for (i = 0; i < num_dgs; ++i) {
		phs[i].priv = dgs[i];
		phs[i].slot = dslots_hash_dgid(dslots, dgs[i]->dgid);
	}
	qsort(phs, num_dgs, sizeof(struct dg_placeholder),
			compare_dg_placeholder);
	i = 0;
	while (1) {
		if (i >= num_dgs)
			break;
		dg = phs[i].priv;
		s = phs[i].slot;
		if (prev_s != s) {
			if (prev_s != -1)
				pthread_mutex_unlock(&dslots->
					slot[prev_s].lock);
			pthread_mutex_lock(&dslots->slot[s].lock);
		}
		SLIST_INSERT_HEAD(&dslots->slot[s].dg_head, dg, entry);
		prev_s = s;
		++i;
	}
	if (prev_s != -1)
		pthread_mutex_unlock(&dslots->slot[prev_s].lock);
	free(phs);
	return 0;
}

int dslots_remove(struct dslots *dslots, uint64_t *dgids, int num_dgs)
{
	struct dg_list dg_head;
	struct delegation *dg;
	uint64_t dgid;
	int i = 0, ret = 0, prev_s = -1, s;
	struct dg_placeholder *phs;

	phs = malloc(num_dgs * sizeof(struct dg_placeholder));
	if (!phs)
		return -ENOMEM;
	for (i = 0; i < num_dgs; ++i) {
		phs[i].priv = (void*)(intptr_t)dgids[i];
		phs[i].slot = dslots_hash_dgid(dslots, dgids[i]);
	}
	qsort(phs, num_dgs, sizeof(struct dg_placeholder),
			compare_dg_placeholder);
	i = 0;
	while (1) {
		if (i >= num_dgs)
			break;
		dgid = (int)(intptr_t)phs[i].priv;
		s = phs[i].slot;
		if (prev_s != s) {
			if (prev_s != -1)
				pthread_mutex_unlock(&dslots->
					slot[prev_s].lock);
			pthread_mutex_lock(&dslots->slot[s].lock);
		}
		SLIST_INIT(&dg_head);
		while (1) {
			dg = SLIST_FIRST(&dslots->slot[s].dg_head);
			if (!dg) {
				break;
			}
			SLIST_REMOVE_HEAD(&dslots->slot[s].dg_head, entry);
			if (dg->dgid == dgid) {
				delegation_free(dg);
				++ret;
			}
			else {
				SLIST_INSERT_HEAD(&dg_head, dg, entry);
			}
		}
		SLIST_SWAP(&dslots->slot[s].dg_head, &dg_head, delegation);
		prev_s = s;
		++i;
	}
	if (prev_s != -1)
		pthread_mutex_unlock(&dslots->slot[prev_s].lock);
	free(phs);
	return ret;
}

struct delegation *dslots_lock(struct dslots *dslots, uint64_t dgid)
{
	struct delegation *dg;
	int s;

	s = dslots_hash_dgid(dslots, dgid);
	pthread_mutex_lock(&dslots->slot[s].lock);
	SLIST_FOREACH(dg, &dslots->slot[s].dg_head, entry) {
		if (dg->dgid == dgid)
			return dg;
	}
	pthread_mutex_unlock(&dslots->slot[s].lock);
	return NULL;
}

void dslots_unlock(struct dslots *dslots, const struct delegation *dg)
{
	int s;

	s = dslots_hash_dgid(dslots, dg->dgid);
	pthread_mutex_unlock(&dslots->slot[s].lock);
}

void dslots_free(struct dslots *dslots)
{
	int i;

	for (i = 0; i < dslots->num_dslots; ++i) {
		pthread_mutex_destroy(&dslots->slot[i].lock);
	}
	free(dslots);
}
