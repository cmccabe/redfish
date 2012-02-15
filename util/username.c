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

#include "util/username.h"
#include "util/string.h"

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int get_current_username(char *out, size_t out_len)
{
	int ret;
	char *buf = NULL;
	struct passwd pwd, *p;
	long buf_len;
	uid_t euid;

	/* It would be nice to use cuserid here, but that function does
	 * different things on different systems and has a lot of weird
	 * limitations.  Even its own man page says that it sucks. */

	/* First get UID */
	euid = geteuid();

	/* How long of a buffer should we use to read password file entries? */
	buf_len = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (buf_len == -1)
		buf_len = 1048576;
	buf = malloc(buf_len);
	if (!buf) {
		ret = -ENOMEM;
		goto done;
	}
	ret = getpwuid_r(euid, &pwd, buf, buf_len, &p);
	if (ret) {
		char *user_env;
		/* Ok, that failed.  Is the USER environment variable set,
		 * though? */
		user_env = getenv("USER");
		if (!user_env) {
			ret = -ENOSYS;
			goto done;
		}
		ret = zsnprintf(out, out_len, "%s", user_env);
	}
	else {
		/* Get the username from the pwd file entry */
		ret = zsnprintf(out, out_len, "%s", pwd.pw_name);
	}
done:
	free(buf);
	return ret;
}

int get_user_id(const char *username, uid_t *uid)
{
	char buf[8192];
	struct passwd pwd, *res;
	int ret;

	ret = getpwnam_r(username, &pwd, buf, sizeof(buf), &res);
	if (res == NULL) {
		if (!ret)
			return -ENOENT;
		return ret;
	}
	*uid = pwd.pw_uid;
	return 0;
}

int get_user_name(uid_t uid, char *username, size_t username_len)
{
	char buf[8192];
	struct passwd pwd, *res;
	int ret;

	ret = getpwuid_r(uid, &pwd, buf, sizeof(buf), &res);
	if (res == NULL) {
		if (!ret)
			return -ENOENT;
		return ret;
	}
	return zsnprintf(username, username_len, "%s", pwd.pw_name);
}

int get_group_id(const char *groupname, gid_t *gid)
{
	char buf[8192];
	struct group grp, *res;
	int ret;

	ret = getgrnam_r(groupname, &grp, buf, sizeof(buf), &res);
	if (res == NULL) {
		if (!ret)
			return -ENOENT;
		return ret;
	}
	*gid = grp.gr_gid;
	return 0;
}

int get_group_name(uid_t uid, char *groupname, size_t groupname_len)
{
	char buf[8192];
	struct group grp, *res;
	int ret;

	ret = getgrgid_r(uid, &grp, buf, sizeof(buf), &res);
	if (res == NULL) {
		if (!ret)
			return -ENOENT;
		return ret;
	}
	return zsnprintf(groupname, groupname_len, "%s", grp.gr_name);
}
