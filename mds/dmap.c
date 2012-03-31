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

#include "mds/dmap.h"
#include "mds/const.h"
#include "msg/types.h"
#include "util/error.h"
#include "util/queue.h"
#include "util/tree.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int dmap_compare(const struct dmap *a, const struct dmap *b) PURE; 
RB_HEAD(child_dmap, dmap);

struct dmap {
	RB_ENTRY(dmap) entry;
	/** child nodes */
	struct child_dmap child_head;
	/** delegation ID */
	uint64_t dgid;
	/** Path component of this node */
	char pcomp[0];
};

RB_GENERATE(child_dmap, dmap, entry, dmap_compare);

static int dmap_compare(const struct dmap *a, const struct dmap *b)
{
	return strcmp(a->pcomp, b->pcomp);
}

struct dmap *dmap_alloc(void)
{
	struct dmap *dmap;
	
	dmap = calloc(1, sizeof(struct dmap) + 1);
	if (!dmap)
		return ERR_PTR(ENOMEM);
	RB_INIT(&dmap->child_head);
	dmap->dgid = RF_ROOT_DGID;
	dmap->pcomp[0] = '\0';
	return dmap;
}

void dmap_free(struct dmap *dmap)
{
	struct dmap *child, *child_tmp;

	RB_FOREACH_SAFE(child, child_dmap, &dmap->child_head, child_tmp) {
		dmap_free(child);
	}
	free(dmap);
}

uint64_t dmap_lookup(struct dmap *dmap, const char *path)
{
	char epath[RF_PATH_MAX], *pcomp;
	size_t i, cur, epath_len, pcomp_len;
	char buf[sizeof(struct dmap) + RF_PATH_MAX + 1] = { 0 };
	struct dmap *exemplar = (struct dmap*)buf;
	struct dmap *child;

	snprintf(epath, sizeof(epath), "%s", path);
	epath_len = strlen(epath);
	for (i = 0; i < epath_len; ++i) {
		if (epath[i] == '/')
			epath[i] = '\0';
	}

	cur = 1;
	while (1) {
		if (cur >= epath_len)
			return dmap->dgid;
		pcomp = epath + cur;
		pcomp_len = strlen(pcomp);
		strcpy(exemplar->pcomp, pcomp);
		child = RB_FIND(child_dmap, &dmap->child_head, exemplar);
		if (!child) {
			return dmap->dgid;
		}
		cur += pcomp_len + 1;
		dmap = child;
	}
}

int dmap_add(struct dmap *dmap, const char *path, uint64_t dgid)
{
	char epath[RF_PATH_MAX], *pcomp;
	size_t i, cur, epath_len, pcomp_len;
	char buf[sizeof(struct dmap) + RF_PATH_MAX + 1] = { 0 };
	struct dmap *exemplar = (struct dmap*)buf;
	struct dmap *child;

	snprintf(epath, sizeof(epath), "%s", path);
	epath_len = strlen(epath);
	for (i = 0; i < epath_len; ++i) {
		if (epath[i] == '/')
			epath[i] = '\0';
	}

	cur = 1;
	while (1) {
		if (cur >= epath_len) {
			if (dmap->dgid != RF_INVAL_DGID)
				return -EEXIST;
			dmap->dgid = dgid;
			return 0;
		}
		pcomp = epath + cur;
		pcomp_len = strlen(pcomp);
		strcpy(exemplar->pcomp, pcomp);
		child = RB_FIND(child_dmap, &dmap->child_head, exemplar);
		if (!child) {
			child = calloc(1, sizeof(struct dmap) + pcomp_len + 1);
			if (!child)
				return -ENOMEM;
			RB_INIT(&child->child_head);
			strcpy(child->pcomp, pcomp);
			RB_INSERT(child_dmap, &dmap->child_head, child);
			if (cur + pcomp_len + 1 >= epath_len) {
				child->dgid = dgid;
				return 0;
			}
			else {
				child->dgid = RF_INVAL_DGID;
			}
		}
		cur += pcomp_len + 1;
		dmap = child;
	}
}

static int dmap_has_single_child(struct dmap *dmap)
{
	struct dmap *child;

	child = RB_MIN(child_dmap, &dmap->child_head);
	if (!child)
		return 0;
	child = RB_NEXT(child_dmap, dmap->child_head, child);
	if (!child)
		return 1;
	return 0;
}

static int dmap_has_children(struct dmap *dmap)
{
	return !!RB_MIN(child_dmap, &dmap->child_head);
}

int dmap_remove(struct dmap *dmap, const char *path)
{
	char epath[RF_PATH_MAX], *pcomp;
	size_t i, cur, epath_len, pcomp_len;
	char buf[sizeof(struct dmap) + RF_PATH_MAX + 1] = { 0 };
	struct dmap *exemplar = (struct dmap*)buf;
	struct dmap *child, *parent = NULL, *mark_parent = NULL, *mark = NULL;

	snprintf(epath, sizeof(epath), "%s", path);
	epath_len = strlen(epath);
	for (i = 0; i < epath_len; ++i) {
		if (epath[i] == '/')
			epath[i] = '\0';
	}

	/* The dmap sometimes has 'placeholder nodes' to represent directories
	 * that may not themselves be the delegations, but which contain other
	 * directories which are.  So if we had delegations { /,  /a/b }, we'd
	 * have a placeholder node for /a.  This placeholder would be deleted if
	 * the /a/b delegation was deleted.
	 *
	 * That's the purpose of mark and mark_parent-- to keep track of the
	 * node we should start our deletion spree at.
	 */
	cur = 1;
	while (1) {
		if (cur >= epath_len) {
			if (dmap_has_children(dmap)) {
				if (parent == NULL)
					return -EINVAL;
				if (dmap->dgid == RF_INVAL_DGID)
					return -ENOENT;
				dmap->dgid = RF_INVAL_DGID;
				return 0;
			}
			if (!mark) {
				mark = dmap;
				mark_parent = parent;
			}
			if (mark_parent == NULL) {
				/* Can't delete root delegation */
				return -EINVAL;
			}
			RB_REMOVE(child_dmap, &mark_parent->child_head, mark);
			dmap_free(mark);
			return 0;
		}
		pcomp = epath + cur;
		pcomp_len = strlen(pcomp);
		strcpy(exemplar->pcomp, pcomp);
		if ((dmap->dgid != RF_INVAL_DGID) ||
				(!dmap_has_single_child(dmap))) {
			mark = NULL;
			mark_parent = NULL;
		}
		else if (mark == NULL) {
			mark = dmap;
			mark_parent = parent;
		}
		child = RB_FIND(child_dmap, &dmap->child_head, exemplar);
		if (!child)
			return -ENOENT;
		cur += pcomp_len + 1;
		parent = dmap;
		dmap = child;
	}
}
