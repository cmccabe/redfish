/*
 * Copyright 2011-2012 the RedFish authors
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

#include "util/platform/readdir.h"

#include <dirent.h>
#include <errno.h>
#include <sys/types.h>

/* readdir is thread-safe on Linux, under glibc
 *
 * What a sane idea.
 */

int  do_opendir(const char *name, struct redfish_dirp** dp)
{
	DIR *rdp = opendir(name);
	if (!rdp)
		return -errno;
	*dp = (struct redfish_dirp*)rdp;
	return 0;
}

struct dirent *do_readdir(struct redfish_dirp *dp)
{
	DIR *rdp = (DIR*)dp;

	return readdir(rdp);
}

void do_closedir(struct redfish_dirp *dp)
{
	DIR *rdp = (DIR*)dp;
	closedir(rdp);
}
