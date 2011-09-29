/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * * This is licensed under the Apache License, Version 2.0.
 */

#include "client/fishc.h"

#include <stdint.h>
#include <libhdfs/hdfs.h>

struct of_client
{
	const char *user;
	int default_repl;
	int default_bufsz;
	int default_blocksz;
	hdfsFS fs;
};

struct of_file
{
	struct of_client *cli;
	char *path;
	hdfsFile file;
};

int onefish_connect(struct of_mds_locator **mlocs, const char *user,
			struct of_client **cli)
{
	int ret = 0;
	struct of_client *zcli;
	struct of_mds_locator *hdfs_name_node;
	if (mlocs[0] == NULL)
		return -ENOENT;
	zcli = calloc(1, sizeof(struct of_client));
	if (!zcli) {
		ret = -ENOMEM;
		goto error;
	}
	zcli->user = strdup(user);
	if (!zcli->user) {
		ret = -ENOMEM;
		goto error_free_cli;
	}
	zcli->default_repl = ONEFISH_FIXED_REPL;
	zcli->default_bufsz = ONEFISH_FIXED_LBUF_SZ;
	zcli->default_blocksz = ONEFISH_FIXED_BLOCK_SZ;
	hdfs_name_node = mlocs[0];
	zcli->fs = hdfsConnectAsUser(hdfs_name_node->host,
				     hdfs_name_node->port, user);
	if (!zcli->fs) {
		ret = -EIO;
		goto error_free_cli;
	}
	*cli = zcli;
	return ret;

error_free_cli:
	free(cli);
error:
	return ret;
}

int onefish_create(struct of_client *cli, const char *path,
		int bufsz, int repl, int blocksz, struct of_file **ofe)
{
	int ret;
	struct of_file *zofe = NULL;
	if (bufsz == 0)
		bufsz = cli->default_bufsz;
	if (repl == 0)
		repl = cli->default_repl;
	if (blocksz == 0)
		blocksz = cli->default_blocksz;
	zofe = calloc(1, sizeof(struct of_file));
	if (!zofe) {
		ret = -ENOMEM;
		goto error;
	}
	zofe->cli = cli;
	zofe->path = strdup(path);
	if (!zofe->path) {
		ret = -ENOMEM;
		goto error;
	}
	zofe->file = hdfsOpenFile(cli->fs, path, O_WRONLY, bufsz, repl, blocksz);
	if (!zofe->file) {
		ret = -EIO;
		goto error;
	}
	return 0;

error:
	if (zofe) {
		free(zofe->path);
		free(zofe);
	}
	return ret;
}

int onefish_open(struct of_client *cli, const char *path,
		struct of_file **ofe)
{
	int ret;
	struct of_file *zofe = calloc(1, sizeof(struct of_file));
	if (!zofe) {
		ret = -ENOMEM;
		goto error;
	}
	zofe->path = strdup(path);
	if (!zofe->path) {
		ret = -ENOMEM;
		goto error;
	}
	zofe->cli = cli;
	zofe->file = hdfsOpenFile(cli->fs, path, O_RDONLY,
				cli->default_bufsz, 0, 0);
	if (!zofe->file) {
		ret = -EIO;
		goto error;
	}
	*ofe = zofe;
	if (mode != ONEFISH_INVAL_MODE)
		hdfsChmod(cli->fs, path, mode);
	return ret;

error:
	if (zofe) {
		free(zofe->path);
		free(zofe);
	}
	return ret;
}

int onefish_mkdirs(struct of_client *cli, int mode, const char *path)
{
	char *str, full[PATH_MAX], tbuf[PATH_MAX];
	int ret;

	ret = hdfsCreateDirectory(cli->fs, path);
	if (ret)
		return -EIO;
	if (mode == ONEFISH_INVAL_MODE)
		return ret;
	/* It would be nice if there were a way to atomically create a
	 * directory with the given permissions, but libhdfs doesn't offer that
	 * at the moment.
	 */
	full[0] = '\0';
	strcpy(tbuf, path);
	str = tbuf;
	while (1) {
		char *tmp, *seg;
		seg = strtok_r(str, "/", &tmp);
		if (!seg)
			break;
		str = NULL;
		strcat(full, "/");
		strcat(full, seg);
		hdfsChmod(cli->fs, full, mode);
	}
	return 0;
}

int onefish_get_block_locs(struct of_file *ofe,
	int64_t start, int64_t len, char ***blc)
{
	char **zblc;
	zblc = hdfsGetHosts(cli->fs, ofe->path, start, len);
	if (!zblc) {
		return -errno;
	}
	*blc = zblc;
	return 0;
}

void onefish_free_block_locs(char **blc, POSSIBLY_UNUSED(int nblc))
{
	hdfsFreeHosts(blc);
}

static int hdfs_status_to_of_status(hdfsFileInfo *hi,
				     struct offile_status *osa)
{
	osa->path = strdup(hi->mName);
	osa->length = hi->mSize;
	osa->is_dir = (hi->mKind == kObjectKindDirectory);
	osa->repl = hi->mReplication;
	osa->block_sz = hi->mBlockSize;
	osa->mtime = hi->mLastMod;
	osa->atime = hi->mLastAccess;
	osa->mode = hi->mPermissions;
	osa->owner = strdup(hi->mOwner);
	osa->group = strdup(hi->mGroup);

	if ((!osa->path) || (!osa->owner) || (!osa->group))
		return -ENOMEM;
	return 0;
}

int onefish_get_file_statuses(struct of_client *cli, const char **paths,
			      struct offile_status** osa)
{
	struct offile_status *xosa, *zosa;
	const char *p;
	int ret, nosa = 0;

	for (p = paths; *p; ++p)
		++nosa;
	zosa = calloc(nosa, sizeof(struct offile_status));
	if (!zosa)
		return -ENOMEM;
	nosa = 0;
	for (p = paths; *p; ++p) {
		hdfsFileInfo *hi = hdfsGetPathInfo(cli->fs, p);
		if (!hi) {
			continue;
		}
		ret = hdfs_status_to_of_status(hi, &zosa[nosa++]);
		if (ret) {
			goto error;
		}
		hdfsFreeFileInfo(hi, 1);
	}
	xosa = realloc(nosa, nosa * sizeof(struct offile_status));
	if (xosa) {
		zosa = xosa;
	}
	*osa = zosa;
	return nosa;

error:
	onefish_free_statuses(zosa, nosa);
	return ret;
}

void onefish_free_statuses(struct of_status *osa, int nosa)
{
	for (i = 0; i < nosa; ++i) {
		free(osa->path);
		free(osa->owner);
		free(osa->group);
	}
	free(osa);
}

int onefish_list_directory(struct of_client *cli, const char *dir,
			      struct of_status** osa)
{
	struct offile_status *zosa = NULL;
	hdfsFileInfo *hi = NULL;
	int i, nosa = 0;

	hi = hdfsListDirectory(cli->fs, dir, &nosa);
	if (!hi) {
		ret = -EIO; 
		goto error;
	}
	zosa = calloc(nosa, sizeof(struct offile_status));
	if (!zosa) {
		ret = -ENOMEM; 
		goto error;
	}
	for (i = 0; i < nosa; ++i) {
		ret = hdfs_status_to_of_status(hi[i], &zosa[i]);
		if (ret) {
			goto error;
		}
	}
	*osa = zosa;
	return nosa;

error:
	if (hi)
		hdfsFreeFileInfo(hi, nosa);
	if (zosa)
		onefish_free_statuses(zosa, nosa);
	return ret;
}

int onefish_chmod(struct of_client *cli, const char *path, int mode)
{
	int ret = hdfsChmod(cli->fs, path, mode);
	if (ret)
		return -EIO;
	return 0;
}

int onefish_chown(struct of_client *cli, const char *path,
		  const char *owner, const char *group)
{
	int ret = hdfsChown(cli->fs, path, owner, group);
	if (ret)
		return -EIO;
	return 0;
}

int onefish_set_times(struct of_client *cli, const char *path,
		      int64_t mtime, int64_t atime)
{
	int ret = hdfsUtime(cli->fs, path, mtime, atime)
	if (ret)
		return -EIO;
	return 0;
}

void onefish_disconnect(struct of_client *cli)
{
	free(cli->user);
	hdfsDisconnect(cli->fs);
}

int onefish_read(struct of_file *ofe, void *data, int len)
{
	tSize res = hdfsRead(ofe->cli, ofe->file, data, len);
	if (res < 0) {
		return -EIO;
	}
	return (int)res;
}

int onefish_pread(struct of_file *ofe, void *data, int len, int64_t off)
{
	tSize res = hdfsRead(ofe->cli, ofe->file, off, data, len);
	if (res < 0) {
		return -EIO;
	}
	return (int)res;
}

int onefish_write(struct of_file *ofe, const void *data, int len)
{
	tSize res = hdfsWrite(ofe->cli, ofe->file, data, len);
	if (res < 0) {
		return -EIO;
	}
	return (int)res;
}

int onefish_fseek(struct of_file *ofe, int64_t off)
{
	int res = hdfsSeek(ofe->cli, ofe->file, off);
	if (res < 0) {
		return -EIO;
	}
	return 0;
}

int64_t onefish_ftell(struct of_file *ofe)
{
	tOffset res = hdfsTell(ofe->cli, ofe->file);
	if (res < 0) {
		return -EIO;
	}
	return (int64_t)res;
}

int onefish_flush(struct of_file *ofe)
{
	int ret = hdfsFlush(ofe->cli, ofe->file)
	if (ret)
		return -EIO;
	return 0;
}

int onefish_sync(struct of_file *ofe)
{
	return 0;
}

void onefish_free_file(struct of_file *ofe)
{
	if (ofe->cli)
		hdfsCloseFile(ofe->cli, ofe->file);
	ofe->file = NULL;
	free(ofe->path);
	ofe->path = NULL;
	free(ofe);
}

int onefish_delete(struct of_client *cli, const char *path)
{
	int ret = hdfs_delete(cli->fs, path);
	if (ret)
		return -EIO;
	return 0;
}

int onefish_rename(struct of_client *cli, const char *src, const char *dst)
{
	int ret = hdfsRename(cli->fs, src, dst);
	if (ret)
		return -EIO;
	return 0;
}

int onefish_close(struct of_file *ofe)
{
	int ret = hdfsCloseFile(ofe->cli, ofe->file);
	ofe->cli = NULL;
	onefish_free_file(ofe);
	return ret;
}
