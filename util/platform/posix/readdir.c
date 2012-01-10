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
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

struct redfish_dirp
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
int  do_opendir(const char *name, struct redfish_dirp** dp)
{
	long name_max;
	size_t buf_sz;
	struct redfish_dirp* zdp;
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
	zdp = calloc(1, sizeof(struct redfish_dirp) + buf_sz);
	if (!zdp) {
		closedir(rdp);
		return -ENOMEM;
	}
	zdp->dp = rdp;
	*dp = zdp;
	return 0;
}

struct dirent *do_readdir(struct redfish_dirp *dp)
{
	int ret;
	struct dirent *result;
	ret = readdir_r(dp->dp, (struct dirent*)dp->dirent_buf, &result);
	if (ret)
		return NULL;
	return result;
}

void do_closedir(struct redfish_dirp *dp)
{
	closedir(dp->dp);
	free(dp);
}
