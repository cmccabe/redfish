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

#include "client/stub/xattrs.h"
#include "util/error.h"
#include "util/str_to_int.h"
#include "util/string.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>

#define INT_BUF_SZ 32

#define TEST_XATTR_NAME "user.my_xattr"
#define TEST_XATTR_VAL "my_xattr_val"
#define TEST_XATTR_VAL_SZ (sizeof(TEST_XATTR_VAL) - 1)

static int check_xattr_support_impl(const char *fname)
{
	ssize_t res;
	char buf[TEST_XATTR_VAL_SZ] = { 0 };

	if (setxattr(fname, TEST_XATTR_NAME, TEST_XATTR_VAL,
			TEST_XATTR_VAL_SZ, 0) < 0) {
		printf("setxattr got %d\n", errno);
		return -errno;
	}
	res = getxattr(fname, TEST_XATTR_NAME, buf, sizeof(buf));
	if (res < 0)
		return -errno;
	if (res != TEST_XATTR_VAL_SZ)
		return -EIO;
	if (memcmp(TEST_XATTR_VAL, buf, TEST_XATTR_VAL_SZ))
		return -EIO;
	return 0;
}

int check_xattr_support(const char *base)
{
	int ret, fd;
	char fname[PATH_MAX];
	if (zsnprintf(fname, sizeof(fname), "%s/.xattr.test", base))
		return -ENAMETOOLONG;
	fd = open(fname, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd < 0)
		return -errno;
	RETRY_ON_EINTR(ret, close(fd));
	ret = check_xattr_support_impl(fname);
	unlink(fname);
	return ret;
}

int xgets(const char *epath, const char *xname, size_t buf_sz, char **x)
{
	ssize_t res;
	char buf[buf_sz + 1];
	res = getxattr(epath, xname, buf, buf_sz);
	if (res < 0)
		return -errno;
	buf[res] = '\0';
	*x = strdup(buf);
	if (!*x)
		return -ENOMEM;
	return 0;
}

int fxgets(int fd, const char *xname, size_t buf_sz, char **x)
{
	ssize_t res;
	char buf[buf_sz + 1];
	res = fgetxattr(fd, xname, buf, buf_sz);
	if (res < 0)
		return -errno;
	buf[res] = '\0';
	*x = strdup(buf);
	if (!*x)
		return -ENOMEM;
	return 0;
}

int xsets(const char *epath, const char *xname, const char *s)
{
	size_t s_len;
	s_len = strlen(s);
	if (setxattr(epath, xname, s, s_len, 0) < 0)
		return -errno;
	return 0;
}

int fxsets(int fd, const char *xname, const char *s)
{
	size_t s_len;
	s_len = strlen(s);
	if (fsetxattr(fd, xname, s, s_len, 0) < 0)
		return -errno;
	return 0;
}

int xgeti(const char *epath, const char *xname, int *x)
{
	ssize_t res;
	int ret;
	char err[1] = { 0 }, buf[INT_BUF_SZ];
	res = getxattr(epath, xname, buf, sizeof(buf) - 1);
	if (res < 0)
		return -errno;
	buf[res] = '\0';
	ret = str_to_int(buf, err, sizeof(err));
	if (err[0])
		return -EINVAL;
	*x = ret;
	return 0;
}

int fxgeti(int fd, const char *xname, int *x)
{
	ssize_t res;
	int ret;
	char err[1] = { 0 }, buf[INT_BUF_SZ];
	res = fgetxattr(fd, xname, buf, sizeof(buf) - 1);
	if (res < 0)
		return -errno;
	buf[res] = '\0';
	ret = str_to_int(buf, err, sizeof(err));
	if (err[0])
		return -EINVAL;
	*x = ret;
	return 0;
}

static int xset_fill_buf(int base, int i, char *buf, size_t buf_len)
{
	char fmt[16];
	switch (base) {
	case 8:
		snprintf(fmt, sizeof(fmt), "0%%o");
		break;
	case 10:
		snprintf(fmt, sizeof(fmt), "%%d");
		break;
	case 16:
		snprintf(fmt, sizeof(fmt), "0x%%x");
		break;
	default:
		return -ENOTSUP;
	}
	snprintf(buf, buf_len, fmt, i);
	return 0;
}

int xseti(const char *epath, const char *xname, int base, int i)
{
	int ret;
	size_t b_len;
	char buf[INT_BUF_SZ];

	ret = xset_fill_buf(base, i, buf, sizeof(buf));
	if (ret)
		return ret;
	b_len = strlen(buf);
	if (setxattr(epath, xname, buf, b_len, 0) < 0)
		return -errno;
	return 0;
}

int fxseti(int fd, const char *xname, int base, int i)
{
	int ret;
	size_t b_len;
	char buf[INT_BUF_SZ];

	ret = xset_fill_buf(base, i, buf, sizeof(buf));
	if (ret)
		return ret;
	b_len = strlen(buf);
	if (fsetxattr(fd, xname, buf, b_len, 0) < 0)
		return -errno;
	return 0;
}
