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

