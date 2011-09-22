/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/platform/readdir.h"

#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

struct onefish_dirp
{
	DIR *dp;
	char dirent_buf[0];
};

/* POSIX doesn't say that readdir is thread-safe, so if we're going to be
 * POSIX-compliant, we're going to have to use readdir_r instead. But, oh nos,
 * nobody will tell us how big a buffer readdir_r needs. And if you give it one
 * that's too small, users can cause a buffer overflow.
 *
 * We can figure out the maximum size we'll need by checking out the maximum
 * path length possible on the filesystem where we're calling opendir(3).
 */
int  do_opendir(const char *name, struct onefish_dirp** dp)
{
	long name_max;
	size_t buf_sz;
	struct onefish_dirp* zdp;
	DIR *rdp = opendir(name);
	if (!rdp)
		return -errno;
	name_max = fpathconf(dirfd(rdp), _PC_NAME_MAX);
	if (name_max < 0) {
		name_max = PATH_MAX;
	}
	buf_sz = offsetof(struct dirent, d_name) + name_max + 1;
	if (buf_sz < sizeof(struct dirent))
		buf_sz = sizeof(struct dirent);
	zdp = calloc(1, sizeof(struct onefish_dirp) + buf_sz);
	if (!zdp) {
		closedir(rdp);
		return -ENOMEM;
	}
	zdp->dp = rdp;
	*dp = zdp;
	return 0;
}

struct dirent *do_readdir(struct onefish_dirp *dp)
{
	int ret;
	struct dirent *result;
	ret = readdir_r(dp->dp, (struct dirent*)dp->dirent_buf, &result);
	if (ret)
		return NULL;
	return result;
}

void do_closedir(struct onefish_dirp *dp)
{
	closedir(dp->dp);
	free(dp);
}
