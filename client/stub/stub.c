/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * * This is licensed under the Apache License, Version 2.0.
 */

#include "client/fishc.h"
#include "client/fishc_internal.h"
#include "client/stub/xattrs.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/macro.h"
#include "util/path.h"
#include "util/platform/readdir.h"
#include "util/safe_io.h"
#include "util/string.h"

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

enum fish_open_ty {
	FISH_OPEN_TY_RD,
	FISH_OPEN_TY_WR,
};

#define WRITE_SHIFT 2
#define READ_SHIFT 4

#define XATTR_FISH_USER "user.fish_user"
#define XATTR_FISH_GROUP "user.fish_group"
#define XATTR_FISH_MODE "user.fish_mode"

/** Represents a OneFish client.
 *
 * You can treat the data fields in this structure as constant between
 * onefish_connect and onefish_disconnect. In other words, in order to login as
 * a new user, or change our groups, we need to create a new connection-- which
 * seems reasonable.
 */
struct of_client
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

/** Represents a OneFish file */
struct of_file
{
	/** Type of open file */
	enum fish_open_ty ty;

	/** Backing file descriptor */
	int fd;
};

static int get_stub_path(const struct of_client *cli, const char *path,
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

static void onefish_free_ofclient(struct of_client *cli)
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

int onefish_connect(POSSIBLY_UNUSED(struct of_mds_locator **mlocs),
			const char *user, struct of_client **cli)
{
	int ret;
	struct of_client *zcli = calloc(1, sizeof(struct of_client));
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
	ret = check_xattr_support(zcli->base);
	if (ret)
		goto oom_error;
	*cli = zcli;
	return 0;

oom_error:
	onefish_free_ofclient(zcli);
	return -ENOMEM; 
}

static int cli_is_group_member(const struct of_client *cli, const char *group)
{
	char **g;
	for (g = cli->group; *g; ++g) {
		if (strcmp(*g, group) == 0)
			return 1;
	}
	return 0;
}

static int validate_perm(const struct of_client *cli, const char *fname,
			 int shift)
{
	int mode;
	if (xgeti(fname, XATTR_FISH_MODE, 8, &mode)) {
		return -EIO;
	}
	/* everyone */
	if (mode & (1 << shift))
		return 0;
	/* user */
	if (mode & (1 << (shift + 6))) {
		int ret;
		char *user;
		if (xgets(fname, XATTR_FISH_USER, ONEFISH_USERNAME_MAX,
				&user)) {
			return -EIO;
		}
		ret = strcmp(cli->user, user);
		free(user);
		if (ret == 0)
			return 0;
	}
	/* group */
	if (mode & (1 << (shift + 3))) {
		int ret;
		char *group;
		if (xgets(fname, XATTR_FISH_GROUP, ONEFISH_GROUPNAME_MAX,
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

static int recursive_validate_perm(const struct of_client *cli,
				const char *fname, int shift)
{
	int ret;
	char *str, full[PATH_MAX], tbuf[PATH_MAX];

	full[0] = '\0';
	strcpy(tbuf, fname);
	str = tbuf;
	while (1) {
		char *tmp, *seg;
		seg = strtok_r(str, "/", &tmp);
		if (!seg)
			break;
		str = NULL;
		strcat(full, "/");
		strcat(full, seg);
		ret = validate_perm(cli, full, shift);
		if (ret)
			return ret;
	}
	return 0;
}

static int set_mode(int fd, int mode)
{
	return fxseti(fd, XATTR_FISH_USER, 8, mode);
}

static int set_user(int fd, const char *user)
{
	return fxsets(fd, XATTR_FISH_USER, user);
}

static int set_group(int fd, const char *group)
{
	return fxsets(fd, XATTR_FISH_USER, group);
}

int onefish_create(struct of_client *cli, const char *path,
	int mode, POSSIBLY_UNUSED(int bufsz), POSSIBLY_UNUSED(int repl),
	POSSIBLY_UNUSED(int blocksz), struct of_file **ofe)
{
	int ret, fd = -1;
	char epath[PATH_MAX], edir[PATH_MAX];
	struct of_file *zofe = NULL;

	ret = get_stub_path(cli, path, epath, PATH_MAX);
	if (ret)
		return ret;
	do_dirname(epath, edir, PATH_MAX);
	pthread_mutex_lock(&cli->lock);
	ret = recursive_validate_perm(cli, edir, WRITE_SHIFT);
	if (ret)
		goto error;
	fd = open(epath, O_WRONLY, 0644);
	if (fd < 0) {
		ret = -errno;
		if (ret == -EEXIST)
			goto error;
		ret = -EIO;
		goto error;
	}
	if (mode == ONEFISH_INVAL_MODE)
		mode = ONEFISH_DEFAULT_FILE_MODE;
	ret = set_mode(fd, mode);
	if (ret)
		goto error;
	ret = set_user(fd, cli->user);
	if (ret)
		goto error;
	ret = set_group(fd, cli->group[0]);
	if (ret)
		goto error;
	zofe = malloc(sizeof(struct of_file));
	if (!zofe) {
		ret = -ENOMEM;
		goto error;
	}
	zofe->ty = FISH_OPEN_TY_WR;
	zofe->fd = fd;
	*ofe = zofe;
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

int onefish_open(struct of_client *cli, const char *path, struct of_file **ofe)
{
	int ret, fd = -1;
	char epath[PATH_MAX];
	struct of_file *zofe = NULL;

	ret = get_stub_path(cli, path, epath, PATH_MAX);
	if (ret)
		return ret;
	ret = recursive_validate_perm(cli, epath, READ_SHIFT);
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
	zofe = malloc(sizeof(struct of_file));
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

int onefish_mkdirs(struct of_client *cli, const char *path, int mode)
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
		seg = strtok_r(str, "/", &tmp);
		if (!seg)
			break;
		str = NULL;
		strcat(full, "/");
		strcat(full, seg);
		if (mkdir(full, 0755) < 0) {
			ret = -errno;
			if (ret == -EEXIST)
				continue;
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
	ret = 0;

done:
	pthread_mutex_unlock(&cli->lock);
	return ret;
}

int onefish_get_block_locs(POSSIBLY_UNUSED(struct of_file *ofe),
		POSSIBLY_UNUSED(int64_t start), POSSIBLY_UNUSED(int64_t len),
		char ***blc)
{
	char **zblc = malloc(1 * sizeof(char *));
	if (!zblc)
		return -ENOMEM;
	zblc[0] = "localhost";
	*blc = zblc;
	return 1;
}

void onefish_free_block_locs(char **blc, int nblc)
{
	if (nblc != 1)
		abort();
	free(blc);
}

static int get_path_status(const char *path, struct of_path_status *zosa)
{
	struct stat sbuf;
	if (stat(path, &sbuf) < 0) {
		return -EIO;
	}
	zosa->path = strdup(path);
	zosa->length = sbuf.st_size;
	zosa->is_dir = !!S_ISDIR(sbuf.st_mode);
	zosa->repl = ONEFISH_FIXED_REPL;
	zosa->block_sz = ONEFISH_FIXED_BLOCK_SZ;
	zosa->mtime = sbuf.st_mtime;
	zosa->atime = sbuf.st_atime;
	zosa->mode = sbuf.st_mode;
	zosa->owner = strdup("cmccabe");
	zosa->group = strdup("cmccabe");
	if ((!zosa->path) || (!zosa->owner) || (!zosa->group)) {
		free(zosa->path);
		free(zosa->owner);
		free(zosa->group);
		return -ENOMEM;
	}
	return 0;
}

int onefish_get_path_statuses(struct of_client *cli, const char **paths,
				struct of_path_status** osa)
{
	struct of_path_status* zosa;
	const char **p;
	int ret, i, cur_file, num_files = 0;

	for (p = paths; *p; ++p) {
		num_files++;
	}
	zosa = calloc(num_files, sizeof(struct of_path_status));
	if (!zosa)
		return -ENOMEM;
	pthread_mutex_lock(&cli->lock);
	cur_file = 0;
	for (i = 0; i < num_files; ++i) {
		char tmp[PATH_MAX];
		get_stub_path(cli, *p, tmp, PATH_MAX);
		ret = get_path_status(tmp, &zosa[cur_file]);
		if (ret) {
			onefish_free_path_statuses(zosa, cur_file);
			goto done;
		}
		cur_file++;
	}
	if (cur_file != num_files) {
		struct of_path_status *xosa;
		xosa = realloc(zosa, cur_file * sizeof(struct of_path_status));
		if (xosa)
			zosa = xosa;
	}
	ret = cur_file;
done:
	pthread_mutex_unlock(&cli->lock);
	if (ret >= 0)
		*osa = zosa;
	return ret;
}

void onefish_free_path_statuses(struct of_path_status* osa, int nosa)
{
	int i;
	for (i = 0; i < nosa; ++i) {
		free(osa[i].path);
		free(osa[i].owner);
		free(osa[i].group);
	}
	free(osa);
}

int onefish_list_directory(struct of_client *cli, const char *dir,
			      struct of_path_status** osa)
{
	struct onefish_dirp *dp = NULL;
	char epath[PATH_MAX];
	int i, ret, nosa = 0;
	struct of_path_status *zosa = NULL;

	if (dir[0] != '/')
		return -EDOM;
	if (zsnprintf(epath, PATH_MAX, "%s%s", cli->base, dir))
		return -ENAMETOOLONG;
	canonicalize_path(epath);
	pthread_mutex_lock(&cli->lock);
	ret = do_opendir(epath, &dp);
	if (ret) {
		pthread_mutex_unlock(&cli->lock);
		return ret;
	}
	while (1) {
		char fname[PATH_MAX];
		struct of_path_status *xosa;
		struct dirent *de;
		de = do_readdir(dp);
		if (!de)
			break;
		if (strcmp(de->d_name, "."))
			continue;
		else if (strcmp(de->d_name, ".."))
			continue;
		++nosa;
		xosa = realloc(zosa, nosa * sizeof(struct of_path_status));
		if (!xosa) {
			ret = -ENOMEM;
			goto free_zosa;
		}
		zosa = xosa;
		if (zsnprintf(fname, PATH_MAX, "%s/%s", epath, de->d_name)) {
			ret = -ENAMETOOLONG;
			goto free_zosa;
		}
		ret = get_path_status(fname, &zosa[nosa - 1]);
		if (ret) {
			nosa--;
		}
	}
	do_closedir(dp);
	pthread_mutex_unlock(&cli->lock);
	*osa = zosa;
	return nosa;
free_zosa:
	if (dp != NULL)
		do_closedir(dp);
	pthread_mutex_unlock(&cli->lock);
	for (i = 0; i <= nosa; ++i) {
		free(zosa[i].path);
		free(zosa[i].owner);
		free(zosa[i].group);
	}
	free(zosa);
	return ret;
}

static int onefish_openwr_internal(struct of_client *cli, const char *path)
{
	int ret, fd = -1;
	char epath[PATH_MAX];

	ret = get_stub_path(cli, path, epath, PATH_MAX);
	if (ret)
		return FORCE_NEGATIVE(ret);
	ret = recursive_validate_perm(cli, epath, WRITE_SHIFT);
	if (ret)
		return FORCE_NEGATIVE(ret);
	fd = open(epath, O_WRONLY);
	if (fd < 0) {
		return -errno;
	}
	return fd;
}

int onefish_set_permission(struct of_client *cli, const char *path, int mode)
{
	int fd, res, ret;

	pthread_mutex_lock(&cli->lock);
	fd = onefish_openwr_internal(cli, path);
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

int onefish_chown(struct of_client *cli, const char *path,
		  const char *owner, const char *group)
{
	int fd, res, ret;

	pthread_mutex_lock(&cli->lock);
	fd = onefish_openwr_internal(cli, path);
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

//int onefish_set_times(struct of_client *cli, const char *path,
//		      int64_t mtime, int64_t atime);

void onefish_disconnect(struct of_client *cli)
{
	onefish_free_ofclient(cli);
}

int onefish_read(struct of_file *ofe, uint8_t *data, int len)
{
	if (ofe->ty != FISH_OPEN_TY_RD)
		return -ENOTSUP;
	return safe_read(ofe->fd, data, len);
}

int onefish_pread(struct of_file *ofe, uint8_t *data, int len, int64_t off)
{
	if (ofe->ty != FISH_OPEN_TY_RD)
		return -ENOTSUP;
	return safe_pread(ofe->fd, data, len, off);
}

int onefish_write(struct of_file *ofe, const uint8_t *data, int len)
{
	if (ofe->ty != FISH_OPEN_TY_WR)
		return -ENOTSUP;
	return safe_write(ofe->fd, data, len);
}

int onefish_fseek(struct of_file *ofe, int64_t off)
{
	off_t res;
	if (ofe->ty != FISH_OPEN_TY_RD)
		return -ENOTSUP;
	res = lseek(ofe->fd, off, SEEK_SET);
	if (res < 0)
		return -EIO;
	return 0;
}

int64_t onefish_ftell(struct of_file *ofe)
{
	off_t res;
	res = lseek(ofe->fd, 0, SEEK_CUR);
	if (res < 0)
		return -EIO;
	return res;
}

int onefish_flush(struct of_file *ofe)
{
	int res;
	RETRY_ON_EINTR(res, close(ofe->fd));
	return res;
}

int onefish_sync(POSSIBLY_UNUSED(struct of_file *ofe))
{
	return 0;
}

void onefish_free_file(struct of_file *ofe)
{
	free(ofe);
}

int onefish_delete(struct of_client *cli, const char *path)
{
	int ret;
	char epath[PATH_MAX];

	ret = get_stub_path(cli, path, epath, PATH_MAX);
	if (ret)
		return ret;
	pthread_mutex_lock(&cli->lock);
	ret = unlink(epath);
	pthread_mutex_unlock(&cli->lock);
	return ret;
}

int onefish_rename(struct of_client *cli, const char *src, const char *dst)
{
	int ret;
	char esrc[PATH_MAX], edst[PATH_MAX], eddir[PATH_MAX];

	pthread_mutex_lock(&cli->lock);
	ret = get_stub_path(cli, src, esrc, PATH_MAX);
	if (ret)
		goto done;
	ret = get_stub_path(cli, dst, edst, PATH_MAX);
	if (ret)
		goto done;
	ret = recursive_validate_perm(cli, esrc, WRITE_SHIFT);
	if (ret)
		goto done;
	do_dirname(edst, eddir, PATH_MAX);
	ret = recursive_validate_perm(cli, eddir, WRITE_SHIFT);
	if (ret)
		goto done;
	ret = rename(esrc, edst);
done:
	pthread_mutex_unlock(&cli->lock);
	if (ret == 0)
		return ret;
	return FORCE_NEGATIVE(ret);
}

int onefish_close(struct of_file *ofe)
{
	int ret;
	ret = onefish_flush(ofe);
	onefish_free_file(ofe);
	return ret;
}
