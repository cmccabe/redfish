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
#include <sys/types.h>

/* readdir is thread-safe on Linux, under glibc
 *
 * What a sane idea.
 */

int  do_opendir(const char *name, struct onefish_dirp** dp)
{
	DIR *rdp = opendir(name);
	if (!rdp)
		return -errno;
	*dp = (struct onefish_dirp*)rdp;
	return 0;
}

struct dirent *do_readdir(struct onefish_dirp *dp)
{
	DIR *rdp = (DIR*)dp;

	return readdir(rdp);
}

void do_closedir(struct onefish_dirp *dp)
{
	DIR *rdp = (DIR*)dp;
	closedir(rdp);
}
