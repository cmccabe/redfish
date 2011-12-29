/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "mds/limits.h"
#include "mds/user.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/packed.h"
#include "util/string.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

PACKED(
struct packed_udata {
	uint32_t num_user;
	uint32_t num_group;
	uint32_t next_uid;
	uint32_t next_gid;
	/* users */
	/* groups */
});

PACKED(
struct packed_user {
	/** user ID */
	uint32_t uid;
	/** primary group ID */
	uint32_t gid;
	/** number of secondary groups */
	uint32_t num_segid;
	/** secondary groups */
	char data[0];
	/** user name */
});

PACKED(
struct packed_group {
	/** group ID */
	uint32_t gid;
	/** group name */
	char data[0];
});

static int user_compare(struct user *a, struct user *b)
{
	return strcmp(a->name, b->name);
}

static int group_compare(struct group *a, struct group *b)
{
	return strcmp(a->name, b->name);
}

RB_HEAD(users, user);
RB_GENERATE(users, user, entry, user_compare);

RB_HEAD(groups, group);
RB_GENERATE(groups, group, entry, group_compare);

struct udata {
	/** All users, sorted by ID */
	struct users users_head;
	/** All groups, sorted by ID */
	struct groups groups_head;
};

struct udata *udata_alloc(void)
{
	struct udata *udata;

	udata = calloc(1, sizeof(struct udata));
	if (!udata)
		return NULL;
	RB_INIT(&udata->users_head);
	RB_INIT(&udata->groups_head);
	return udata;
}

void udata_free(struct udata *udata)
{
	struct user *u, *u_tmp;
	struct group *g, *g_tmp;

	RB_FOREACH_SAFE(u, users, &udata->users_head, u_tmp) {
		RB_REMOVE(users, &udata->users_head, u);
		free(u);
	}
	RB_FOREACH_SAFE(g, groups, &udata->groups_head, g_tmp) {
		RB_REMOVE(groups, &udata->groups_head, g);
		free(g);
	}
	free(udata);
}

int user_in_gid(const struct user *user, uint32_t gid)
{
	size_t i;
	uint32_t num_segid;

	if (user->gid == gid)
		return 1;
	num_segid = user->num_segid;
	for (i = 0; i < num_segid; ++i) {
		if (user->segid[i] == gid)
			return 1;
	}
	return 0;
}

int user_add_segid(struct user **user, uint32_t segid)
{
	struct user *u, *n;
	uint32_t num_segid;

	u = *user;
	num_segid = u->num_segid;
	for (i = 0; i < num_segid; ++i) {
		if (u->segid[i] == segid)
			return -EEXIST;
	}
	num_segid++;
	n = realloc(u, sizeof(struct user) + (sizeof(uint32_t) * num_segid));
	if (!n)
		return -ENOMEM;
	n[num_segid] = segid;
	*user = n;
	return 0;
}

const struct user *udata_lookup_user(struct udata *udata,
		const char *name)
{
	struct user *user, exemplar;

	memset(&exemplar, 0, sizeof(exemplar));
	if (zsnprintf(exemplar.name, RF_USER_MAX, "%s", name))
		return ERR_PTR(ENAMETOOLONG);
	user = RB_FIND(users, &udata->users_head, &exemplar);
	if (!user)
		return ERR_PTR(ENOENT);
	return user;
}

const struct group *udata_lookup_group(struct udata *udata,
		const char *name)
{
	struct group *group, exemplar;

	memset(&exemplar, 0, sizeof(exemplar));
	if (zsnprintf(exemplar.name, RF_USER_MAX, "%s", name))
		return ERR_PTR(ENAMETOOLONG);
	group = RB_FIND(groups, &udata->groups_head, &exemplar);
	if (!group)
		return ERR_PTR(ENOENT);
	return group;
}

struct user* udata_add_user(struct udata *udata, const char *name,
		uint32_t uid, uint32_t gid)
{
	int ret;
	struct user *user, *prev_user;

	user = calloc(1, sizeof(struct user));
	if (!user)
		return ERR_PTR(ENOMEM);
	if (gid == RF_INVAL_GID)
		return ERR_PTR(EINVAL);
	if (uid == RF_INVAL_UID)
		uid = udata->next_uid;
	user->uid = uid;
	ret = zsnprintf(user->name, RF_USER_MAX, "%s", name);
	if (ret) {
		free(user);
		return ERR_PTR(ENAMETOOLONG);
	}
	user->gid = gid;
	user->num_segid = 0;
	prev_user = RB_INSERT(users, &udata->users_head, u);
	if (prev_user != NULL) {
		free(user);
		return ERR_PTR(EEXIST);
	}
	if (uid >= udata->next_uid)
		udata->next_uid = uid + 1;
	return user;
}

struct group* udata_add_group(struct udata *udata, const char *name,
		uint32_t gid)
{
	int ret;
	struct group *group, *prev_group;

	group = calloc(1, sizeof(struct group) + strlen(name) + 1);
	if (!group)
		return ERR_PTR(ENOMEM);
	if (gid == RF_INVAL_GID)
		gid = udata->next_gid;
	group->gid = gid;
	strcpy(group->name, name);
	prev_group = RB_INSERT(groups, &udata->groups_head, group);
	if (prev_group != NULL) {
		free(group);
		return ERR_PTR(EEXIST);
	}
	if (gid >= udata->next_gid)
		udata->next_gid = gid + 1;
	return group;
}

int pack_user(const struct user *user, char *buf,
		uint32_t *off, uint32_t max)
{
	int ret;
	uint32_t o, need, num_segid, i;
	struct packed_user *hdr;

	o = *off;
	need = sizeof(struct packed_user) +
			 (sizeof(uint32_t) * user->num_segid);
	if (max - o < need)
		return -ENAMETOOLONG;
	hdr = (struct packed_user*)(buf + o);
	o += need;
	pack_to_be32(&hdr->uid, user->uid);
	pack_to_be32(&hdr->gid, user->gid);
	num_segid = user->num_segid;
	pack_to_be32(&hdr->num_segid, num_segid);
	for (i = 0; i < num_segid; ++i) {
		pack_to_be32(hdr->data + o, user->segid[i]);
		o += sizeof(uint32_t);
	}
	ret = pack_str(buf, &o, max, user->name);
	if (ret)
		return ret;
	*off = o;
	return 0;
}

int pack_group(const struct group *group, char *buf,
		uint32_t *off, uint32_t max)
{
	int ret;
	uint32_t o, need;
	struct packed_group *hdr;

	o = *off;
	need = sizeof(struct packed_user);
	if (max - o < need)
		return -ENAMETOOLONG;
	hdr = (struct packed_group*)(buf + o);
	o += need;
	pack_to_be32(&hdr->gid, group->gid);
	ret = pack_str(buf, &o, max, group->name);
	if (ret)
		return ret;
	*off = o;
	return 0;
}

int pack_udata(struct udata *udata, char *buf,
		uint32_t *off, uint32_t max)
{
	int ret;
	uint32_t o, need, num_user = 0, num_group = 0;
	struct packed_udata *hdr;
	struct user *u;
	struct group *g;

	o = *off;
	need = sizeof(struct packed_udata);
	if (max - o < need)
		return -ENAMETOOLONG;
	hdr = (struct packed_udata*)(buf + o);
	o += need;
	RB_FOREACH(u, users, &udata->users_head) {
		ret = pack_user(u, buf, &o, max);
		if (ret)
			return ret;
		num_user++;
	}
	RB_FOREACH(g, groups, &udata->groups_head) {
		ret = pack_group(g, buf, &o, max);
		if (ret)
			return ret;
		num_group++;
	}
	pack_to_be32(&hdr->num_user, num_user);
	pack_to_be32(&hdr->num_group, num_group);
	pack_to_be32(&hdr->next_uid, udata->next_uid);
	pack_to_be32(&hdr->next_gid, udata->next_gid);
	*off = o;
	return 0;
}

struct user *unpack_user(char *buf, uint32_t *off, uint32_t max)
{
	int ret;
	uint32_t o, num_segid, i;
	struct packed_user *hdr;
	struct user *user;

	o = *off;
	if (max - o < sizeof(struct packed_user))
		return ERR_PTR(ENAMETOOLONG);
	hdr = (struct packed_user*)(buf + o);
	o += sizeof(struct packed_user);
	num_segid = unpack_from_be32(&hdr->num_segid);
	if (max - o < sizeof(uint32_t) * num_segid)
		return ERR_PTR(ENAMETOOLONG);
	user = calloc(1, sizeof(struct user) + (sizeof(uint32_t) * num_segid));
	if (!user)
		return ERR_PTR(ENOMEM);
	user->uid = unpack_from_be32(&hdr->uid);
	user->gid = unpack_from_be32(&hdr->gid);
	user->num_segid = num_segid;
	ret = unpack_str(buf, &o, max, user->name, RF_USER_MAX);
	if (ret) {
		free(user);
		return ERR_PTR(FORCE_POSITIVE(ret));
	}
	for (i = 0; i < num_segid; ++i) {
		user->segid[i] = unpack_from_be32(buf + o);
		o += sizeof(uint32_t);
	}
	*off = o;
	return user;
}

struct group *unpack_group(char *buf, uint32_t *off, uint32_t max)
{
	int ret;
	uint32_t o;
	struct packed_group *hdr;
	struct group *group;
	char name[RF_GROUP_MAX];

	o = *off;
	if (max - o < sizeof(struct packed_group))
		return ERR_PTR(ENAMETOOLONG);
	hdr = (struct packed_group*)(buf + o);
	o += sizeof(struct packed_group);
	ret = unpack_str(buf, &o, max, name, RF_GROUP_MAX);
	if (ret)
		return ERR_PTR(FORCE_POSITIVE(ret));
	group = malloc(sizeof(struct group) + strlen(name) + 1);
	if (!group)
		return ERR_PTR(ENOMEM);
	group->gid = unpack_from_be32(&hdr->gid);
	ret = zsnprintf(group->name, RF_GROUP_MAX, "%s", name);
	if (ret) {
		free(group);
		return ERR_PTR(FORCE_POSITIVE(ret));
	}
	return group;
}

struct udata* unpack_udata(char *buf, uint32_t *off, uint32_t max)
{
	uint32_t o, need, num_user, num_group, i;
	struct packed_udata *hdr;
	struct udata *udata;
	struct group *g;
	struct user *u;

	o = *off;
	need = sizeof(struct packed_udata);
	if (max - o < need)
		return ERR_PTR(ENAMETOOLONG);
	udata = udata_alloc();
	if (!udata)
		return ERR_PTR(ENOMEM);
	hdr = (struct packed_udata*)(buf + o);
	o += need;
	udata->num_user = num_user = unpack_from_be32(&hdr->num_user);
	udata->num_group = num_group = unpack_from_be32(&hdr->num_group);
	udata->next_uid = unpack_from_be32(&hdr->next_uid);
	udata->next_gid = unpack_from_be32(&hdr->next_gid);
	for (i = 0; i < num_user; ++i) {
		u = unpack_user(buf, &o, max);
		if (IS_ERR(u)) {
			udata_free(udata);
			return (struct udata*)u;
		}
		RB_INSERT(users, &udata->users_head, u);
	}
	for (i = 0; i < num_group; ++i) {
		g = unpack_group(buf, &o, max);
		if (IS_ERR(g)) {
			udata_free(udata);
			return (struct udata*)g;
		}
		RB_INSERT(groups, &udata->groups_head, g);
	}
	return udata;
}
