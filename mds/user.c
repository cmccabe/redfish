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

#include <string.h>

struct udata {
	int i;
};
static struct udata blah;

struct udata *udata_create(void)
{
	// TODO: use fgetpwent or similar to find system users, and store the
	// information here.
	return (struct udata*)&blah;
}

void udata_free(POSSIBLY_UNUSED(struct udata *udata))
{
	// do nothing
}

int user_in_group(POSSIBLY_UNUSED(const struct udata *udata),
		const char *user, const char *group)
{
	return (!strcmp(user, group));
}

const char *user_primary_group(POSSIBLY_UNUSED(const struct udata *udata),
		const char *user)
{
	return user;
}
