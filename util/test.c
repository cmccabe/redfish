/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/test.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void die_unless(int t)
{
  if (!t)
    abort();
}

void die_if(int t)
{
  if (t)
    abort();
}

int do_touch1(const char *fname)
{
	FILE *fp;
	fp = fopen(fname, "w");
	if (fp == NULL) {
		return -errno;
	}
	fclose(fp);
	return 0;
}

int do_touch2(const char *dir, const char *fname)
{
	FILE *fp;
	size_t res;
	char path[PATH_MAX];
	res = snprintf(path, PATH_MAX, "%s/%s", dir, fname);
	if (res >= PATH_MAX)
		return -ENAMETOOLONG;
	fp = fopen(path, "w");
	if (fp == NULL)
		return -errno;
	fclose(fp);
	return 0;
}

