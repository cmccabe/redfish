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

#include "client/fishc.h"
#include "client/fishc_internal.h"
#include "client/stub/xattrs.h"
#include "mds/defaults.h"
#include "mds/limits.h"
#include "util/compiler.h"
#include "util/dir.h"
#include "util/error.h"
#include "util/macro.h"
#include "util/path.h"
#include "util/platform/readdir.h"
#include "util/run_cmd.h"
#include "util/safe_io.h"
#include "util/string.h"
#include "util/username.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

enum fish_open_ty {
	FISH_OPEN_TY_RD,
	FISH_OPEN_TY_WR,
};

enum stub_perm {
	STUB_VPERM_EXE = 0x1,
	STUB_VPERM_WRITE = 0x2,
	STUB_VPERM_READ = 0x4,
};

#define XATTR_FISH_USER "user.fish_user"
#define XATTR_FISH_GROUP "user.fish_group"
#define XATTR_FISH_MODE "user.fish_mode"

void redfish_release_client(struct redfish_client *cli);

/** Represents a Redfish client.
 *
 * You can treat the data fields in this structure as constant between
 * redfish_connect and redfish_disconnect. In other words, in order to login as
 * a new user, or change our groups, we need to create a new connection-- which
 * seems reasonable.
 */
struct redfish_client
{
	/** User name. */
	char *user;

	/** NULL-terminated list of groups we're a member of. The first group
	 * is the primary group. */
	char **group;

	/** Base directory of stub filesystem. */
	char *base;

	/** Lock for the stub filesystem */
	pthread_mutex_t lock;
};

/** Represents a Redfish file */
struct redfish_file
{
	/** Type of open file */
	enum fish_open_ty ty;

	/** Backing file descriptor */
	int fd;
};

static int get_stub_path(const struct redfish_client *cli, const char *path,
			 char *epath, size_t epath_len)
{
	if (path[0] != '/')
		return -EDOM;
	if (zsnprintf(epath, epath_len, "%s%s", cli->base, path))
		return -ENAMETOOLONG;
	canonicalize_path(epath);
	return 0;
}

/** Get the groups that a user is in.
 *
 * For now, assume each user is only a member of the group bearing his name
 */
static char** get_user_groups(const char *user)
{
	char **r = calloc(2, sizeof(char*));
	if (!r)
		return NULL;
	r[0] = strdup(user);
	if (!r[0]) {
		free(r);
		return NULL;
	}
	return r;
}

static const char* get_stub_base_dir(void)
{
	const char *sb = getenv("STUB_BASE");
	if (!sb)
		return "/tmp/stub_base";
	else
		return sb;
}

void redfish_mkfs(POSSIBLY_UNUSED(const char *uconf),
	POSSIBLY_UNUSED(uint16_t mid), POSSIBLY_UNUSED(uint64_t fsid),
	char *err, size_t err_len)
{
	int ret;
	const char *base;

	base = get_stub_base_dir();
	ret = do_mkdir_p(base, MSTOR_ROOT_NID_INIT_MODE);
	if (ret) {
		snprintf(err, err_len, "do_mkdir_p failed with error "
			"%d", ret);
		return;
	}
	/* set up owner/group/mode on the root */
	ret = xseti(base, XATTR_FISH_MODE, 8, MSTOR_ROOT_NID_INIT_MODE);
	if (ret) {
		snprintf(err, err_len, "failed to set mode "
			"on '%s': error %d", base, ret);
		return;
	}
	ret = xsets(base, XATTR_FISH_USER, RF_SUPERUSER_NAME);
	if (ret) {
		snprintf(err, err_len, "failed to set user "
			"on '%s': error %d", base, ret);
		return;
	}
	ret = xsets(base, XATTR_FISH_GROUP, RF_SUPERUSER_NAME);
	if (ret) {
		snprintf(err, err_len, "failed to set group "
			"on '%s': error %d", base, ret);
		return;
	}
}

int redfish_connect(POSSIBLY_UNUSED(const char *conf_path),
			const char *user, struct redfish_client **cli)
{
	int ret;
	struct redfish_client *zcli = calloc(1, sizeof(struct redfish_client));
	if (!zcli)
		return -ENOMEM;
	ret = pthread_mutex_init(&zcli->lock, NULL);
	if (ret) {
		free(zcli);
		return -ENOMEM;
	}
	zcli->user = strdup(user);
	if (!zcli->user)
		goto oom_error;
	zcli->group = get_user_groups(user);
	if (!zcli->group)
		goto oom_error;
	zcli->base = strdup(get_stub_base_dir());
	if (!zcli->base)
		goto oom_error;
	if (access(zcli->base, R_OK | W_OK | X_OK)) {
		redfish_release_client(zcli);
		return -ENOTDIR;
	}
	ret = check_xattr_support(zcli->base);
	if (ret) {
		redfish_release_client(zcli);
		return ret;
	}
	*cli = zcli;
	return 0;

oom_error:
	redfish_release_client(zcli);
	return -ENOMEM;
}

static int cli_is_group_member(const struct redfish_client *cli, const char *group)
{
	char **g;
	for (g = cli->group; *g; ++g) {
		if (strcmp(*g, group) == 0)
			return 1;
	}
	return 0;
}

static int validate_perm(const struct redfish_client *cli, const char *fname,
			 int want)
{
	int mode;

	/** The superuser can skip permission checks */
	if (!strcmp(cli->user, RF_SUPERUSER_NAME))
		return 0;
	if (xgeti(fname, XATTR_FISH_MODE, 8, &mode)) {
		return -EIO;
	}
	/* everyone */
	if (mode & want)
		return 0;
	/* user */
	if (mode & (want << 6)) {
		int ret;
		char *user;

		if (xgets(fname, XATTR_FISH_USER, RF_USER_MAX,
				&user)) {
			return -EIO;
		}
		ret = strcmp(cli->user, user);
		free(user);
		if (ret == 0)
			return 0;
	}
	/* group */
	if (mode & (want << 3)) {
		int ret;
		char *group;
		if (xgets(fname, XATTR_FISH_GROUP, RF_GROUP_MAX,
				&group)) {
			return -EIO;
		}
		ret = cli_is_group_member(cli, group);
		free(group);
		if (ret == 1) {
			return 0;
		}
	}
	return -EPERM;
}

static int client_stub_get_npc(const char *path)
{
	int i, npc = 0;

	/* special case: / has 0 path components */
	if ((path[0] == '/') && (path[1] == '\0'))
		return 0;
	for (i = 0; path[i]; ++i) {
		if (path[i] == '/')
			npc++;
	}
	return npc;
}

static int stub_check_perm(struct redfish_client *cli, const char *epath,
		enum stub_perm want)
{
	struct stat st_buf;
	int ret, cpc = 0, npc;
	char *str, full[PATH_MAX], tbuf[PATH_MAX];
	size_t base_len;

	full[0] = '\0';
	strcpy(tbuf, epath);
	str = tbuf;
	base_len = strlen(cli->base);
	npc = client_stub_get_npc(epath);
	printf("epath='%s', npc = %d\n", epath, npc);
	while (1) {
		char *tmp, *seg;

		++cpc;
		seg = strtok_r(str, "/", &tmp);
		if (!seg)
			break;
		str = NULL;
		strcat(full, "/");
		strcat(full, seg);
		if (strlen(full) < base_len) {
			printf("full = '%s', skipping\n", full);
			continue;
		}
		printf("full = '%s', cpc = %d, testing\n", full, cpc);
		if (cpc == npc) {
			ret = validate_perm(cli, full, want);
			if (ret)
				return ret;
		}
		else {
			if (stat(full, &st_buf) < 0) {
				return -errno;
			}
			if (!S_ISDIR(st_buf.st_mode)) {
				return -ENOTDIR;
			}
			ret = validate_perm(cli, full, STUB_VPERM_EXE);
			if (ret)
				return ret;
		}
	}
	printf("success\n");
	return 0;
}

static int stub_check_file_perm(struct redfish_client *cli,
		const char *epath, enum stub_perm perm)
{
	struct stat st_buf;

	if (strlen(epath) < strlen(cli->base)) {
		return -ENOENT;
	}
	if (stat(epath, &st_buf) < 0) {
		return -errno;
	}
	/* Technically, could be other non-file, but not if all is well. */
	if (!S_ISREG(st_buf.st_mode))
		return -EISDIR;
	return stub_check_perm(cli, epath, perm);
}

static int stub_check_dir_perm(struct redfish_client *cli,
		const char *epath, enum stub_perm perm)
{
	struct stat st_buf;

	if (strlen(epath) < strlen(cli->base)) {
		/* Can always list the root directory. */
		return 0;
	}
	if (stat(epath, &st_buf) < 0) {
		return -errno;
	}
	if (!S_ISDIR(st_buf.st_mode))
		return -ENOTDIR;
	return stub_check_perm(cli, epath, perm);
}

static int stub_check_enc_dir_perm(struct redfish_client *cli,
		const char *epath, enum stub_perm perm)
{
	char edir[PATH_MAX];

	do_dirname(epath, edir, PATH_MAX);
	return stub_check_dir_perm(cli, edir, perm);
}

static int set_mode(int fd, int mode)
{
	return fxseti(fd, XATTR_FISH_MODE, 8, mode);
}

static int set_user(int fd, const char *user)
{
	return fxsets(fd, XATTR_FISH_USER, user);
}

static int set_group(int fd, const char *group)
{
	return fxsets(fd, XATTR_FISH_GROUP, group);
}

int redfish_create(struct redfish_client *cli, const char *path,
	int mode, POSSIBLY_UNUSED(int bufsz), POSSIBLY_UNUSED(int repl),
	POSSIBLY_UNUSED(uint32_t blocksz), struct redfish_file **ofe)
{
	int ret, fd = -1;
	char epath[PATH_MAX];
	struct redfish_file *zofe = NULL;

	ret = get_stub_path(cli, path, epath, PATH_MAX);
	if (ret)
		return ret;
	pthread_mutex_lock(&cli->lock);
	fprintf(stderr, "redfish_create: validating permissions\n");
	ret = stub_check_enc_dir_perm(cli, epath, STUB_VPERM_WRITE);
	if (ret)
		goto error;
	fprintf(stderr, "redfish_create: creating file\n");
	fd = open(epath, O_WRONLY | O_CREAT, 0644);
	if (fd < 0) {
		ret = -errno;
		if (ret == -EEXIST)
			goto error;
		ret = -EIO;
		goto error;
	}
	if (mode == REDFISH_INVAL_MODE)
		mode = REDFISH_DEFAULT_FILE_MODE;
	fprintf(stderr, "redfish_create: setting mode, user, group\n");
	ret = set_mode(fd, mode);
	if (ret)
		goto error;
	ret = set_user(fd, cli->user);
	if (ret)
		goto error;
	ret = set_group(fd, cli->group[0]);
	if (ret)
		goto error;
	zofe = malloc(sizeof(struct redfish_file));
	if (!zofe) {
		ret = -ENOMEM;
		goto error;
	}
	zofe->ty = FISH_OPEN_TY_WR;
	zofe->fd = fd;
	*ofe = zofe;
	fprintf(stderr, "redfish_create: created as '%s'\n", epath);
	pthread_mutex_unlock(&cli->lock);
	return 0;

error:
	if (fd >= 0) {
		RETRY_ON_EINTR(ret, close(fd));
		unlink(epath);
	}
	if (zofe)
		free(zofe);
	pthread_mutex_unlock(&cli->lock);
	return ret;
}

int redfish_open(struct redfish_client *cli, const char *path, struct redfish_file **ofe)
{
	int ret, fd = -1;
	char epath[PATH_MAX];
	struct redfish_file *zofe = NULL;

	ret = get_stub_path(cli, path, epath, PATH_MAX);
	if (ret)
		return ret;
	ret = stub_check_file_perm(cli, epath, STUB_VPERM_READ);
	if (ret)
		return ret;
	pthread_mutex_lock(&cli->lock);
	fd = open(epath, O_RDONLY);
	if (fd < 0) {
		ret = -errno;
		if (ret == -ENOENT)
			goto error;
		ret = -EIO;
		goto error;
	}
	zofe = malloc(sizeof(struct redfish_file));
	if (!zofe) {
		ret = -ENOMEM;
		goto error;
	}
	zofe->ty = FISH_OPEN_TY_RD;
	zofe->fd = fd;
	*ofe = zofe;
	pthread_mutex_unlock(&cli->lock);
	return 0;

error:
	if (zofe)
		free(zofe);
	if (fd >= 0)
		RETRY_ON_EINTR(ret, close(fd));
	pthread_mutex_unlock(&cli->lock);
	return ret;
}

int redfish_mkdirs(struct redfish_client *cli, int mode, const char *path)
{
	int fd, ret;
	char epath[PATH_MAX];
	char *str, full[PATH_MAX], tbuf[PATH_MAX];

	ret = get_stub_path(cli, path, epath, PATH_MAX);
	if (ret)
		return ret;
	pthread_mutex_unlock(&cli->lock);
	/* do_mkdir_p */
	full[0] = '\0';
	strcpy(tbuf, epath);
	str = tbuf;
	while (1) {
		DIR *dp;
		char *tmp, *seg;
		struct stat st_buf;

		seg = strtok_r(str, "/", &tmp);
		if (!seg)
			break;
		str = NULL;
		strcat(full, "/");
		strcat(full, seg);
		if (stat(full, &st_buf) < 0) {
			ret = -errno;
			if (ret != -ENOENT)
				goto done;
			if (mkdir(full, 0755) < 0) {
				ret = -errno;
				goto done;
			}
			dp = opendir(full);
			fd = dirfd(dp);
			ret = set_mode(fd, mode);
			if (ret) {
				closedir(dp);
				goto done;
			}
			ret = set_user(fd, cli->user);
			if (ret) {
				closedir(dp);
				goto done;
			}
			ret = set_group(fd, cli->group[0]);
			if (ret) {
				closedir(dp);
				goto done;
			}
			closedir(dp);
		}
		else if (S_ISDIR(st_buf.st_mode)) {
			ret = stub_check_dir_perm(cli, full, STUB_VPERM_EXE);
			if (ret)
				goto done;
		}
		else {
			ret = -ENOTDIR;
			goto done;
		}
	}
	ret = 0;

done:
	pthread_mutex_unlock(&cli->lock);
	return ret;
}

int redfish_locate(POSSIBLY_UNUSED(struct redfish_client *cli),
		POSSIBLY_UNUSED(const char *path), int64_t start, int64_t len,
		struct redfish_block_loc ***blc)
{
	int ret, nblc = 1;
	struct redfish_block_loc **zblc = NULL;

	zblc = calloc(1, nblc * sizeof(struct redfish_block_loc *));
	if (!zblc) {
		ret = -ENOMEM;
		goto error;
	}
	zblc[0] = calloc(1, sizeof(struct redfish_block_loc) +
			 sizeof(struct redfish_block_host));
	if (!zblc[0]) {
		ret = -ENOMEM;
		goto error;
	}
	zblc[0]->start = start;
	zblc[0]->len = len;
	zblc[0]->nhosts = 1;
	zblc[0]->hosts[0].port = 0;
	zblc[0]->hosts[0].hostname = strdup("localhost");
	if (!zblc[0]->hosts[0].hostname) {
		ret = -ENOMEM;
		goto error;
	}
	*blc = zblc;
	return 1;

error:
	if (zblc)
		redfish_free_block_locs(zblc, nblc);
	return ret;
}

static int st_buf_to_redfish_stat(const struct stat *st_buf,
		struct redfish_stat *zosa)
{
	char owner[RF_USER_MAX], group[RF_GROUP_MAX];

	zosa->length = st_buf->st_size;
	zosa->is_dir = !!S_ISDIR(st_buf->st_mode);
	zosa->repl = REDFISH_FIXED_REPL;
	zosa->block_sz = REDFISH_FIXED_BLOCK_SZ;
	zosa->mtime = st_buf->st_mtime;
	zosa->atime = st_buf->st_atime;
	zosa->nid = st_buf->st_ino;
	zosa->mode = st_buf->st_mode;
	if (get_user_name(st_buf->st_uid, owner, sizeof(owner)))
		snprintf(owner, sizeof(owner), "nobody");
	if (get_group_name(st_buf->st_gid, group, sizeof(group)))
		snprintf(group, sizeof(group), "nobody");
	zosa->owner = strdup(owner);
	if (!zosa->owner)
		return -ENOMEM;
	zosa->group = strdup(group);
	if (!zosa->group) {
		free(zosa->group);
		return -ENOMEM;
	}
	return 0;
}

static int get_path_status(const char *path, struct redfish_stat *zosa)
{
	struct stat st_buf;
	if (stat(path, &st_buf) < 0) {
		return -errno;
	}
	return st_buf_to_redfish_stat(&st_buf, zosa);
}

int redfish_get_path_status(struct redfish_client *cli, const char *path,
				struct redfish_stat* osa)
{
	char epath[PATH_MAX];
	int ret;

	pthread_mutex_lock(&cli->lock);
	get_stub_path(cli, path, epath, PATH_MAX);
	ret = stub_check_enc_dir_perm(cli, epath, STUB_VPERM_READ);
	if (ret)
		goto done;
	ret = get_path_status(epath, osa);
done:
	pthread_mutex_unlock(&cli->lock);
	return ret;
}

int redfish_get_file_status(struct redfish_file *ofe, struct redfish_stat *osa)
{
	int ret;
	struct stat st_buf;

	if (fstat(ofe->fd, &st_buf)) {
		ret = -errno;
		return ret;
	}
	st_buf_to_redfish_stat(&st_buf, osa);
	return 0;
}

int redfish_list_directory(struct redfish_client *cli, const char *path,
			      struct redfish_dir_entry** oda)
{
	struct redfish_dirp *dp = NULL;
	char epath[PATH_MAX];
	int ret, noda = 0;
	struct redfish_dir_entry *zoda = NULL;

	if (path[0] != '/')
		return -EDOM;
	ret = get_stub_path(cli, path, epath, PATH_MAX);
	if (ret)
		return ret;
	printf("redfish_list_directory: path='%s', epath='%s'\n", path, epath);
	pthread_mutex_lock(&cli->lock);
	ret = stub_check_enc_dir_perm(cli, epath, STUB_VPERM_READ);
	if (ret)
		goto error;
	ret = do_opendir(epath, &dp);
	if (ret)
		goto error;
	while (1) {
		char fname[PATH_MAX];
		struct redfish_dir_entry *xoda;
		struct dirent *de;
		struct stat st_buf;

		de = do_readdir(dp);
		if (!de)
			break;
		if (!strcmp(de->d_name, "."))
			continue;
		else if (!strcmp(de->d_name, ".."))
			continue;
		xoda = realloc(zoda,
			(noda + 1) * sizeof(struct redfish_dir_entry));
		if (!xoda) {
			ret = -ENOMEM;
			goto error;
		}
		zoda = xoda;
		if (zsnprintf(fname, PATH_MAX, "%s/%s", epath, de->d_name)) {
			ret = -ENAMETOOLONG;
			goto error;
		}
		if (stat(epath, &st_buf) < 0) {
			ret = -errno;
			goto error;
		}
		ret = st_buf_to_redfish_stat(&st_buf, &zoda[noda].stat);
		if (ret)
			goto error;
		zoda[noda].name = strdup(de->d_name);
		if (!zoda[noda].name) {
			ret = -ENOMEM;
			goto error;
		}
		++noda;
	}
	do_closedir(dp);
	pthread_mutex_unlock(&cli->lock);
	*oda = zoda;
	return noda;
error:
	if (dp != NULL)
		do_closedir(dp);
	pthread_mutex_unlock(&cli->lock);
	redfish_free_dir_entries(zoda, noda);
	return FORCE_NEGATIVE(ret);
}

static int redfish_openwr_internal(struct redfish_client *cli, const char *path)
{
	int ret, fd = -1;
	char epath[PATH_MAX];

	ret = get_stub_path(cli, path, epath, PATH_MAX);
	if (ret)
		return FORCE_NEGATIVE(ret);
	ret = stub_check_file_perm(cli, epath, STUB_VPERM_WRITE);
	if (ret)
		return FORCE_NEGATIVE(ret);
	fd = open(epath, O_WRONLY);
	if (fd < 0) {
		return -errno;
	}
	return fd;
}

int redfish_chmod(struct redfish_client *cli, const char *path, int mode)
{
	int fd, res, ret;

	pthread_mutex_lock(&cli->lock);
	fd = redfish_openwr_internal(cli, path);
	if (fd < 0) {
		ret = fd;
		goto done;
	}
	ret = set_mode(fd, mode);
	if (ret) {
		goto done;
	}
	ret = 0;
done:
	if (fd >= 0)
		RETRY_ON_EINTR(res, close(fd));
	pthread_mutex_unlock(&cli->lock);
	return ret;
}

int redfish_chown(struct redfish_client *cli, const char *path,
		  const char *owner, const char *group)
{
	int fd, res, ret;

	pthread_mutex_lock(&cli->lock);
	fd = redfish_openwr_internal(cli, path);
	if (fd < 0) {
		ret = fd;
		goto done;
	}
	if ((owner) && (owner[0])) {
		ret = set_user(fd, owner);
		if (ret) {
			goto done;
		}
	}
	if ((group) && (group[0])) {
		ret = set_group(fd, group);
		if (ret) {
			goto done;
		}
	}
	ret = 0;
done:
	if (fd >= 0)
		RETRY_ON_EINTR(res, close(fd));
	pthread_mutex_unlock(&cli->lock);
	return ret;
}

int redfish_utimes(struct redfish_client *cli, const char *path,
		      int64_t mtime, int64_t atime)
{
	int ret;
	char epath[PATH_MAX];
	struct utimbuf tbuf;
	memset(&tbuf, 0, sizeof(tbuf));
	tbuf.actime = (time_t)atime;
	tbuf.modtime = (time_t)mtime;
	ret = get_stub_path(cli, path, epath, PATH_MAX);
	if (ret)
		return ret;
	pthread_mutex_lock(&cli->lock);
	ret = stub_check_file_perm(cli, epath, STUB_VPERM_WRITE);
	if (ret)
		return ret;
	ret = utime(epath, &tbuf);
	pthread_mutex_unlock(&cli->lock);
	return ret;
}

void redfish_disconnect(POSSIBLY_UNUSED(struct redfish_client *cli))
{
	/* This doesn't actually do anything, since we're not really connected
	 * to anything when using the stub client. */
}

void redfish_release_client(struct redfish_client *cli)
{
	char **g;
	free(cli->user);
	for (g = cli->group; *g; ++g)
		free(*g);
	free(cli->group);
	free(cli->base);
	pthread_mutex_destroy(&cli->lock);
	free(cli);
}

int redfish_read(struct redfish_file *ofe, void *data, int len)
{
	if (ofe->ty != FISH_OPEN_TY_RD)
		return -ENOTSUP;
	return safe_read(ofe->fd, data, len);
}

int32_t redfish_available(struct redfish_file *ofe)
{
	struct stat sbuf;
	if (fstat(ofe->fd, &sbuf) < 0) {
		return 0;
	}
	if (sbuf.st_size > 0x7fffffff) {
		return 0x7fffffff;
	}
	else {
		return (uint32_t)sbuf.st_size;
	}
}

int redfish_pread(struct redfish_file *ofe, void *data, int len, int64_t off)
{
	if (ofe->ty != FISH_OPEN_TY_RD)
		return -ENOTSUP;
	return safe_pread(ofe->fd, data, len, off);
}

int redfish_write(struct redfish_file *ofe, const void *data, int len)
{
	if (ofe->ty != FISH_OPEN_TY_WR)
		return -ENOTSUP;
	return safe_write(ofe->fd, data, len);
}

int redfish_fseek(struct redfish_file *ofe, int64_t off)
{
	off_t res;
	if (ofe->ty != FISH_OPEN_TY_RD)
		return -ENOTSUP;
	res = lseek(ofe->fd, off, SEEK_SET);
	if (res < 0)
		return -EIO;
	return 0;
}

int64_t redfish_ftell(struct redfish_file *ofe)
{
	off_t res;
	res = lseek(ofe->fd, 0, SEEK_CUR);
	if (res < 0)
		return -EIO;
	return res;
}

int redfish_hflush(POSSIBLY_UNUSED(struct redfish_file *ofe))
{
	return 0;
}

int redfish_hsync(POSSIBLY_UNUSED(struct redfish_file *ofe))
{
	return 0;
}

int redfish_unlink(struct redfish_client *cli, const char *path)
{
	int ret;
	char epath[PATH_MAX];

	ret = get_stub_path(cli, path, epath, PATH_MAX);
	if (ret)
		return ret;
	pthread_mutex_lock(&cli->lock);
	ret = stub_check_enc_dir_perm(cli, epath, STUB_VPERM_WRITE);
	if (ret)
		goto done;
	ret = unlink(epath);
done:
	pthread_mutex_unlock(&cli->lock);
	return ret;
}

int redfish_rmdir(struct redfish_client *cli, const char *path)
{
	int ret;
	char epath[PATH_MAX];

	ret = get_stub_path(cli, path, epath, PATH_MAX);
	if (ret)
		return ret;
	pthread_mutex_lock(&cli->lock);
	ret = stub_check_enc_dir_perm(cli, epath, STUB_VPERM_WRITE);
	if (ret)
		goto done;
	if (rmdir(epath)) {
		ret = -errno;
		goto done;
	}
	ret = 0;
done:
	pthread_mutex_unlock(&cli->lock);
	return ret;
}

int redfish_unlink_tree(struct redfish_client *cli, const char *path)
{
	int ret;
	char epath[PATH_MAX];

	ret = get_stub_path(cli, path, epath, PATH_MAX);
	if (ret)
		return ret;
	pthread_mutex_lock(&cli->lock);
	/* FIXME: this is not the right permission check.  It only checks the
	 * permission of the root of the subtree we're deleting. */
	ret = stub_check_enc_dir_perm(cli, epath, STUB_VPERM_WRITE);
	if (ret)
		goto done;
	ret = run_cmd("rm", "-rf", epath, (char*)NULL);
done:
	pthread_mutex_unlock(&cli->lock);
	return ret;
}

int redfish_rename(struct redfish_client *cli, const char *src, const char *dst)
{
	int ret;
	char esrc[PATH_MAX], edst[PATH_MAX];

	pthread_mutex_lock(&cli->lock);
	ret = get_stub_path(cli, src, esrc, PATH_MAX);
	if (ret)
		goto done;
	if (strlen(esrc) == strlen(cli->base)) {
		/* can't move root directory */
		ret = -EINVAL;
		goto done;
	}
	ret = get_stub_path(cli, dst, edst, PATH_MAX);
	if (ret)
		goto done;
	/* We need to be able to remove the src file from where it is now */
	ret = stub_check_enc_dir_perm(cli, esrc, STUB_VPERM_WRITE);
	if (ret)
		goto done;

	struct stat st_buf;
	if (stat(edst, &st_buf) < 0) {
		ret = -errno;
		if (ret != -ENOENT)
			goto done;
		ret = stub_check_enc_dir_perm(cli, edst, STUB_VPERM_WRITE);
		if (ret)
			goto done;
	}
	else if (S_ISDIR(st_buf.st_mode)) {
		/* Tried to rename into a directory, which is allowed */
		ret = stub_check_dir_perm(cli, edst, STUB_VPERM_WRITE);
		if (ret)
			return ret;
	}
	else {
		if (stat(esrc, &st_buf) < 0)
			ret = -EIO;
		else if (S_ISDIR(st_buf.st_mode)) {
			/* Tried to rename a directory over a file; neither HDFS
			 * nor POSIX semantics allow this */
			ret = -ENOTDIR;
		}
		else if (S_ISREG(st_buf.st_mode)) {
			/* Tried to rename a file over a file; HDFS doesn't
			 * allow this (though POSIX does) */
			ret = -EEXIST;
		}
		else {
			ret = -EIO;
		}
		goto done;
	}
	if (rename(esrc, edst) < 0) {
		ret = -errno;
	}
	else {
		ret = 0;
	}
done:
	pthread_mutex_unlock(&cli->lock);
	return ret;
}

int redfish_close(struct redfish_file *ofe)
{
	int ret;
	RETRY_ON_EINTR(ret, close(ofe->fd));
	ofe->fd = -1;
	return ret;
}

void redfish_free_file(struct redfish_file *ofe)
{
	int ret;

	if (ofe->fd >= 0)
		RETRY_ON_EINTR(ret, close(ofe->fd));
	free(ofe);
}
