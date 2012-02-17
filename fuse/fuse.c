/*
 * Copyright 2012 the Redfish authors
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

#define _ATFILE_SOURCE
#define FUSE_USE_VERSION 26

#include "core/glitch_log.h"
#include "client/fishc.h"
#include "mds/limits.h"
#include "util/cram.h"
#include "util/error.h"
#include "util/safe_io.h"
#include "util/error.h"
#include "util/string.h"
#include "util/username.h"

#include <errno.h>
#include <fcntl.h>
#include <fuse/fuse.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The FUSE filesystem interface for Redfish.
 *
 * A few notes:
 * We do extensive client-side caching here to adapt Redfish's non-POSIX
 * semantics to the POSIX semantics that FUSE expects.  This is not the most
 * efficient way to access Redfish-- it never will be.  But it may be the most
 * convenient way for certain applications.
 *
 * It's ok to use Linux-isms in this code.  FUSE is a Linux-specific interface
 * which doesn't exist on other platforms.  Most operating systems have similar
 * interfaces, but the interfaces are different.
 */

#define FH_TO_FUSE_FILE_RO(fh) ((struct rf_fuse_file_ro*)(uintptr_t)fh)
#define FH_TO_FUSE_FILE_RW(fh) ((struct rf_fuse_file_rw*)(uintptr_t)fh)

/** Option to set Redfish user name */
#define RF_FUSE_USER_OPT "user="

/** Option to set Redfish configuration file */
#define RF_FUSE_CONF_OPT "conf="

struct rf_fuse_fs {
	/** The Redfish client backing this FUSE filesystem */
	struct redfish_client *cli;
	/** Path to backing directory */
	char backing_path[PATH_MAX];
	/** File descriptor for backing directory */
	int dirfd;
	/** Lock that protects next_fid */
	pthread_mutex_t next_fid_lock;
	/** Next file ID to use as a backing file */
	uint64_t next_fid;
};

struct rf_fuse_file_rw {
	/** File ID */
	uint64_t fid;
	/** Backing file descriptor. */
	int fd;
	/** Dirty */
	uint16_t dirty;
	/** Creation mode */
	uint16_t mode;
};

struct rf_fuse_file_ro {
	/** Backing redfish file */
	struct redfish_file *ofe;
};

/** The Redfish filesystem */
static struct rf_fuse_fs *g_fs;

/** Convert a Redfish stat structure into a POSIX stat structure */
static void rf_stat_to_st_buf(const struct redfish_stat *osa,
				struct stat *st_buf)
{
	int ret;
	uid_t uid;
	gid_t gid;

	ret = get_user_id(osa->owner, &uid);
	if (ret)
		uid = 0;
	ret = get_group_id(osa->group, &gid);
	if (ret)
		gid = 0;
	memset(st_buf, 0, sizeof(struct stat));
	st_buf->st_dev = 0;
	st_buf->st_ino = osa->nid;
	st_buf->st_mode = osa->mode;
	st_buf->st_nlink = (osa->is_dir == 0) ? 1 : 2;
	st_buf->st_uid = uid;
	st_buf->st_gid = gid;
	st_buf->st_rdev = 0;
	st_buf->st_size = osa->length;
	st_buf->st_blksize = 512;
	st_buf->st_blocks = osa->length / 512;
	st_buf->st_atime = osa->atime;
	st_buf->st_mtime = osa->mtime;
	st_buf->st_ctime = osa->mtime;
}

static int rf_fuse_getattr(const char *path, struct stat *st_buf)
{
	int ret;
	struct redfish_stat osa;

	ret = redfish_get_path_status(g_fs->cli, path, &osa);
	if (ret)
		return FORCE_NEGATIVE(ret);
	rf_stat_to_st_buf(&osa, st_buf);
	redfish_free_path_status(&osa);
	return 0;
}

static int rf_fuse_mkdir(const char *path, mode_t mode)
{
	return redfish_mkdirs(g_fs->cli, mode, path);
}

static int rf_fuse_unlink(const char *path)
{
	return redfish_unlink(g_fs->cli, path);
}

static int rf_fuse_rmdir(const char *path)
{
	return redfish_rmdir(g_fs->cli, path);
}

static int rf_fuse_rename(const char *src, const char *dst)
{
	return redfish_rename(g_fs->cli, src, dst);
}

static int rf_fuse_chmod(const char *path, mode_t mode)
{
	return redfish_chmod(g_fs->cli, path, mode);
}

static int rf_fuse_chown(const char *path, uid_t uid, gid_t gid)
{
	int ret;
	char username[RF_USER_MAX], groupname[RF_GROUP_MAX];

	ret = get_user_name(uid, username, RF_USER_MAX);
	if (ret)
		return -EUSERS;
	ret = get_group_name(gid, groupname, RF_GROUP_MAX);
	if (ret)
		return -EUSERS;
	return redfish_chown(g_fs->cli, path, username, groupname);
}

static int copy_rf_file_to_local(int fd, struct redfish_file *ofe)
{
	char buf[8192];
	int res, ret;

	while (1) {
		res = redfish_read(ofe, buf, sizeof(buf));
		if (res < 0)
			return res;
		if (res == 0)
			return 0;
		ret = safe_write(fd, buf, res);
		if (ret)
			return FORCE_NEGATIVE(ret);
		if ((size_t)res < sizeof(buf))
			return 0;
	}
}

static int copy_local_file_to_rf(struct redfish_file *ofe, int fd)
{
	char buf[8192];
	int res, ret;

	while (1) {
		res = safe_read(fd, buf, sizeof(buf));
		if (res <= 0)
			return res;
		ret = redfish_write(ofe, buf, res);
		if (ret)
			return ret;
		if ((size_t)res < sizeof(buf))
			return 0;
	}
}

static struct rf_fuse_file_rw* rf_fuse_open_rw(const char *path, int mode, int flags)
{
	struct rf_fuse_file_rw *fo;
	uint64_t next_fid;
	char epath[64] = { 0 };
	struct redfish_file *ofe = NULL;
	struct redfish_stat osa;
	int ret;

	fo = calloc(1, sizeof(struct rf_fuse_file_rw));
	if (!fo) {
		ret = -ENOMEM;
		goto error;
	}
	pthread_mutex_lock(&g_fs->next_fid_lock);
	next_fid = g_fs->next_fid++;
	pthread_mutex_unlock(&g_fs->next_fid_lock);
	snprintf(epath, sizeof(epath), "%" PRId64, next_fid);
	ret = openat(g_fs->dirfd, epath, O_RDWR | O_TRUNC, 0600);
	if (ret < 0) {
		ret = -errno;
		goto error_free_fo;
	}
	fo->fd = ret;

	/* Flags:
	 * O_CREAT and O_TRUNC:
	 * 	Don't open the file at all.  Use the provided mode.
	 * O_CREAT
	 *	Try to open the file and use the existing contents.
	 *	Use the provided mode.
	 * O_TRUNC
	 * 	Open the file or fail.  Use the existing mode.
	 * (none)
	 * 	Open the file or fail.  Load the existing contents.
	 * 	Use the existing mode.
	 */
	if (flags & O_CREAT) {
		if (!(flags & O_TRUNC)) {
			ret = redfish_open(g_fs->cli, path, &ofe);
			if (ret == 0) {
				ret = copy_rf_file_to_local(fo->fd, ofe);
				if (ret) {
					ret = ret;
					goto error_unlink_backing_file;
				}
			}
			else if (ret != -ENOENT) {
				goto error_unlink_backing_file;
			}
			redfish_close(ofe);
		}
		fo->mode = mode;
	}
	else {
		ret = redfish_open(g_fs->cli, path, &ofe);
		if (ret)
			goto error_unlink_backing_file;
		if (!(flags & O_TRUNC)) {
			ret = copy_rf_file_to_local(fo->fd, ofe);
			if (ret) {
				ret = ret;
				goto error_unlink_backing_file;
			}
		}
		ret = redfish_get_file_status(ofe, &osa);
		if (ret)
			goto error_unlink_backing_file;
		fo->mode = osa.mode;
		redfish_free_path_status(&osa);
		redfish_close(ofe);
	}
	return fo;

error_unlink_backing_file:
	unlinkat(g_fs->dirfd, epath, 0);
	RETRY_ON_EINTR(ret, close(fo->fd));
error_free_fo:
	free(fo);
error:
	if (ofe)
		redfish_close(ofe);
	return ERR_PTR(ret);
}

static struct rf_fuse_file_ro* rf_fuse_open_ro(const char *path)
{
	struct rf_fuse_file_ro *fo;
	int ret;

	fo = calloc(1, sizeof(struct rf_fuse_file_ro));
	if (!fo) {
		ret = -ENOMEM;
		goto error;
	}
	ret = redfish_open(g_fs->cli, path, &fo->ofe);
	if (ret) {
		goto error_free_fo;
	}
	return fo;

error_free_fo:
	free(fo);
error:
	return ERR_PTR(ret);
}

static int rf_fuse_open(const char *path, struct fuse_file_info *fi)
{
	if (fi->flags == O_RDONLY) {
		struct rf_fuse_file_ro *fo;

		fo = rf_fuse_open_ro(path);
		if (IS_ERR(fo))
			return PTR_ERR(fo);
		fi->fh = (uint64_t)(uintptr_t)fo;
		return 0;
	}
	else {
		struct rf_fuse_file_rw *fo;

		fo = rf_fuse_open_rw(path, 0, fi->flags);
		if (IS_ERR(fo))
			return PTR_ERR(fo);
		fi->fh = (uint64_t)(uintptr_t)fo;
		return 0;
	}
}

static int rf_fuse_read_direct(struct rf_fuse_file_ro *fo, char *buf,
		size_t amt, off_t off)
{
	size_t rem = amt;
	int ret, cur;

	while (1) {
		if (rem > INT_MAX)
			cur = INT_MAX;
		else
			cur = rem;
		ret = redfish_pread(fo->ofe, buf, cur, off);
		if (ret < 0)
			return ret;
		if (ret < cur)
			return amt - rem;
		off += cur;
		rem -= cur;
		if (cur == 0)
			return amt;
	}
}

static int rf_fuse_read(POSSIBLY_UNUSED(const char *path), char *buf,
		size_t amt, off_t off, struct fuse_file_info *fi)
{
	struct rf_fuse_file_rw *fo;

	if (fi->flags == O_RDONLY)
		return rf_fuse_read_direct(FH_TO_FUSE_FILE_RO(fi->fh),
				buf, amt, off);
	if (fi->flags == O_WRONLY)
		return -EBADF;
	fo = FH_TO_FUSE_FILE_RW(fi->fh);
	return safe_pread(fo->fd, buf, amt, off);
}

static int rf_fuse_write(POSSIBLY_UNUSED(const char *path), const char *buf,
		size_t amt, off_t off, struct fuse_file_info *fi)
{
	struct rf_fuse_file_rw *fo;

	if (fi->flags == O_RDONLY)
		return -EBADF;
	fo = FH_TO_FUSE_FILE_RW(fi->fh);
	fo->dirty = 1;
	return safe_pwrite(fo->fd, buf, amt, off);
}

static int rf_fuse_writeback(const char *path, const struct fuse_file_info *fi,
			int sync)
{
	int ret;
	struct rf_fuse_file_rw *fo;
	struct redfish_file *ofe;

	/* Do I need locking here? */
	if (fi->flags & O_RDONLY)
		return 0;
	fo = FH_TO_FUSE_FILE_RW(fi->fh);
	if (!fo->dirty) {
		/* nothing to do! */
		return 0;
	}
	fo->dirty = 0;
	ret = lseek(fo->fd, 0, SEEK_SET);
	if (ret)
		return -errno;
	ret = redfish_create(g_fs->cli, path, fo->mode, 0, 0, 0, &ofe);
	if (ret)
		return ret;
	ret = copy_local_file_to_rf(ofe, fo->fd);
	if (ret)
		goto done_close_file;
	if (sync) {
		ret = redfish_hsync(ofe);
		if (ret)
			goto done_close_file;
	}
	ret = 0;
done_close_file:
	redfish_close_and_free(ofe);
	return ret;
}

static int rf_fuse_flush(const char *path, struct fuse_file_info *fi)
{
	return rf_fuse_writeback(path, fi, 0);
}

static void free_rf_fuse_file_ro(struct rf_fuse_file_ro *fo)
{
	redfish_close(fo->ofe);
	fo->ofe = NULL;
	free(fo);
}

static void free_rf_fuse_file_rw(struct rf_fuse_file_rw *fo)
{
	int res;
	char epath[64] = { 0 };

	RETRY_ON_EINTR(res, close(fo->fd));
	snprintf(epath, sizeof(epath), "%" PRId64, fo->fid);
	unlinkat(g_fs->dirfd, epath, 0);
	fo->fd = -1;
	free(fo);
}

static int rf_fuse_release(POSSIBLY_UNUSED(const char *path),
		struct fuse_file_info *fi)
{
	if (fi->flags & O_RDONLY)
		free_rf_fuse_file_ro(FH_TO_FUSE_FILE_RO(fi->fh));
	else
		free_rf_fuse_file_rw(FH_TO_FUSE_FILE_RW(fi->fh));
	fi->fh = 0;
	return 0;
}

static int rf_fuse_fsync(const char *path,
		POSSIBLY_UNUSED(int datasync), struct fuse_file_info *fi)
{
	return rf_fuse_writeback(path, fi, 1);
}

static int rf_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	POSSIBLY_UNUSED(off_t off), POSSIBLY_UNUSED(struct fuse_file_info *fi))
{
	int i, noda;
	struct stat st_buf;
	struct redfish_dir_entry *odas;

	noda = redfish_list_directory(g_fs->cli, path, &odas);
	if (noda < 0)
		return noda;
	for (i = 0; i < noda; ++i) {
		rf_stat_to_st_buf(&odas[i].stat, &st_buf);
		filler(buf, odas[i].name, &st_buf, 0);
	}
	redfish_free_dir_entries(odas, noda);
	return 0;
}

static void* rf_fuse_init(struct fuse_conn_info *conn)
{
	conn->async_read = 1;
	conn->max_write = 100 * 1024 * 1024;
	conn->want |= FUSE_CAP_ASYNC_READ;
	conn->want |= FUSE_CAP_ATOMIC_O_TRUNC;
	conn->want |= FUSE_CAP_BIG_WRITES;
	if (!(conn->capable & FUSE_CAP_ATOMIC_O_TRUNC)) {
		glitch_log("WARNING: your kernel does not appear to be "
			   "capable of atomic_o_trunc.  Your performance "
			   "when creating new files will be degraded.\n");
	}
	return NULL;
}

static int rf_fuse_create(const char *path, mode_t mode,
		struct fuse_file_info *fi)
{
	struct rf_fuse_file_rw *fo;

	fo = rf_fuse_open_rw(path, mode, fi->flags | O_CREAT);
	if (IS_ERR(fo))
		return PTR_ERR(fo);
	fi->fh = (uint64_t)(uintptr_t)fo;
	return 0;
}

static int rf_fuse_utimens(const char *path, const struct timespec tv[2])
{
	uint64_t atime, mtime;

	atime = tv[0].tv_sec;
	mtime = tv[1].tv_sec;
	return redfish_utimes(g_fs->cli, path, mtime, atime);
}

static struct fuse_operations g_fishfuse_ops = {
	.getattr = rf_fuse_getattr,
	.readlink = NULL,
	.getdir = NULL, /* deprecated in FUSE */
	.mknod = NULL,
	.mkdir = rf_fuse_mkdir,
	.unlink = rf_fuse_unlink,
	.rmdir = rf_fuse_rmdir,
	.symlink = NULL, /* symlinks are not yet supported */
	.rename = rf_fuse_rename,
	.link = NULL, /* hard links are not yet supported */
	.chmod = rf_fuse_chmod,
	.chown = rf_fuse_chown,
	.truncate = NULL,
	.utime = NULL, /* deprecated in FUSE */
	.open = rf_fuse_open,
	.read = rf_fuse_read,
	.write = rf_fuse_write,
	.statfs = NULL,
	.flush = rf_fuse_flush,
	.release = rf_fuse_release,
	.fsync = rf_fuse_fsync,
	.setxattr = NULL,
	.getxattr = NULL,
	.listxattr = NULL,
	.removexattr = NULL,
	.opendir = NULL,
	.readdir = rf_fuse_readdir,
	.releasedir = NULL,
	.fsyncdir = NULL, /* we don't need fsyncdir */
	.init = rf_fuse_init,
	.destroy = NULL,
	.access = NULL,
	.create = rf_fuse_create,
	.ftruncate = NULL,
	.fgetattr = NULL, /* no equivalent in Redfish */
	.lock = NULL, /* file locks not supported */
	.utimens = rf_fuse_utimens,
	.bmap = NULL,
	.flag_nullpath_ok = 0, /* should change at some point */
	.ioctl = NULL, /* not used */
	.poll = NULL, /* no equivalent in Redfish */
};

static void xsrealloc(char **dst, const char *src)
{
	char *out;
	size_t slen;

	if (!src)
		src = "";
	slen = strlen(src);
	out = realloc(*dst, slen + 1);
	if (!out)
		abort();
	strcpy(out, src);
	*dst = out;
}

static void fishfuse_usage(char *argv0)
{
	int help_argc = 2;
	char *help_argv[] = { argv0, "-ho", NULL };
	struct fuse_args fargs = FUSE_ARGS_INIT(help_argc, help_argv);
	static const char *usage_lines[] = {
"fishfuse: the FUSE connector for Redfish.",
"See http://www.club.cc.cmu.edu/~cmccabe/redfish.html for the most up-to-date",
"information about Redfish.",
"",
"The FUSE connector allows you to access a Redfish filesystem as if it were a",
"local filesystem.",
"",
"USAGE",
"fishfuse [mount point] [options]",
"",
"EXAMPLE USAGE",
"fishfuse /mnt/tmp -o 'large_read,user=foo,conf=/etc/redfish.conf'",
"",
"GENERAL OPTIONS",
"-h, --help            This help message",
"-V                    Print FUSE version",
"",
"REDFISH OPTIONS",
"-o user=<USER>        Set the Redfish user name.",
"-o conf=<PATH>        Set the Redfish configuration file path.",
"",
NULL
	};
	print_lines(stderr, usage_lines);
	fuse_parse_cmdline(&fargs, NULL, NULL, NULL);
	exit(EXIT_FAILURE); /* Should be unreachable.  FUSE exits after printing
				usage information. */
}

static void fishfuse_handle_opts(const char *opts, char **cpath,
		char **user, struct fuse_args *fargs)
{
	char *buf, *tok, *state = NULL, *out = NULL;

	buf = strdup(opts);
	if (!buf)
		abort();
	for (tok = strtok_r(buf, ",", &state); tok;
			(tok = strtok_r(NULL, ";", &state))) {
		if (!strncmp(tok, RF_FUSE_USER_OPT,
				sizeof(RF_FUSE_USER_OPT) - 1)) {
			xsrealloc(user, tok + sizeof(RF_FUSE_USER_OPT) - 1);
		}
		else if (!strncmp(tok, RF_FUSE_CONF_OPT,
				sizeof(RF_FUSE_CONF_OPT) - 1)) {
			xsrealloc(cpath, tok + sizeof(RF_FUSE_CONF_OPT) - 1);
		}
		else if (!out) {
			/* sizeof("-o") includes the NULL byte */
			out = malloc(strlen(opts) + sizeof("-o"));
			if (!out)
				abort();
			strcpy(out, "-o");
			strcat(out, tok);
		}
		else {
			printf("case DD\n");
			strcat(out, ",");
			strcat(out, tok);
		}
	}
	if (out) {
		fargs->argv[fargs->argc++] = out;
	}
	free(buf);
}

static void fishfuse_parse_argv(int argc, char **argv,
		char **cpath, char **user, struct fuse_args *fargs)
{
	int idx;
	size_t olen;

	xsrealloc(cpath, getenv("REDFISH_CONF"));
	fargs->argc = 0;
	fargs->allocated = 1;
	fargs->argv = calloc(argc + 1, sizeof(char*));
	if (!fargs->argv)
		abort();
	fargs->argv[fargs->argc] = strdup(argv[0]);
	if (!fargs->argv[fargs->argc++])
		abort();
	idx = 1;
	while (1) {
		if (idx >= argc)
			break;
		olen = strlen(argv[idx]);

		if ((!strcmp(argv[idx], "-h")) ||
				(!strcmp(argv[idx], "--help"))) {
			fishfuse_usage(argv[0]);
		}
		else if ((olen >= 2) && (argv[idx][0] == '-') &&
				(argv[idx][1] == 'o')) {
			if (olen == 2) {
				if (++idx >= argc) {
					fprintf(stderr, "Missing argument "
						"after -o\n");
					exit(EXIT_FAILURE);
				}
				fishfuse_handle_opts(argv[idx++], cpath,
						user, fargs);
			}
			else {
				fishfuse_handle_opts(argv[idx++] + 2, cpath,
						user, fargs);
			}
		}
		else {
			fargs->argv[fargs->argc++] = strdup(argv[idx++]);
		}
	}
	if (!*cpath) {
		fprintf(stderr, "fishfuse: you must supply a Redfish "
			"configuration file path with -c or the REDFISH_CONF "
			"environment variable.  -h for help.\n");
		exit(EXIT_FAILURE);
	}
}

static struct rf_fuse_fs *rf_fuse_fs_create(const char *cpath,
		const char *user)
{
	int ret;
	struct rf_fuse_fs *fs;

	fs = calloc(1, sizeof(struct rf_fuse_fs));
	if (!fs)
		abort();
	ret = pthread_mutex_init(&fs->next_fid_lock, NULL);
	ret = redfish_connect(cpath, user, &fs->cli);
	if (ret) {
		fprintf(stderr, "fishfuse: failed to connect to Redfish: "
			"error %d", ret);
		exit(EXIT_FAILURE);
	}
	return fs;
}

static void rf_fuse_fs_free(struct rf_fuse_fs *fs)
{
	redfish_disconnect_and_release(fs->cli);
	free(fs);
}

int main(int argc, char *argv[])
{
	char *cpath = NULL, *user = NULL;
	struct fuse_args fargs;
	int ret = 1;

	fishfuse_parse_argv(argc, argv, &cpath, &user, &fargs);
	g_fs = rf_fuse_fs_create(cpath, user);
	ret = fuse_main(fargs.argc, fargs.argv, &g_fishfuse_ops, NULL);
	fuse_opt_free_args(&fargs);
	rf_fuse_fs_free(g_fs);
	free(cpath);
	free(user);
	return cram_into_u8(ret);
}
