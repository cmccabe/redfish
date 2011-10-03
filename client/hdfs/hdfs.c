/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * * This is licensed under the Apache License, Version 2.0.
 */

#include "client/fishc.h"

#include <stdint.h>
#include <libhdfs/hdfs.h>

struct redfish_client
{
	const char *user;
	int default_repl;
	int default_bufsz;
	int default_blocksz;
	hdfsFS fs;
};

struct redfish_file
{
	struct redfish_client *cli;
	char *path;
	hdfsFile file;
};

int redfish_connect(struct redfish_mds_locator **mlocs, const char *user,
			struct redfish_client **cli)
{
	int ret = 0;
	struct redfish_client *zcli;
	struct redfish_mds_locator *hdfs_name_node;
	if (mlocs[0] == NULL)
		return -ENOENT;
	zcli = calloc(1, sizeof(struct redfish_client));
	if (!zcli) {
		ret = -ENOMEM;
		goto error;
	}
	zcli->user = strdup(user);
	if (!zcli->user) {
		ret = -ENOMEM;
		goto error_free_cli;
	}
	zcli->default_repl = REDFISH_FIXED_REPL;
	zcli->default_bufsz = REDFISH_FIXED_LBUF_SZ;
	zcli->default_blocksz = REDFISH_FIXED_BLOCK_SZ;
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

int redfish_create(struct redfish_client *cli, const char *path,
		int bufsz, int repl, int blocksz, struct redfish_file **ofe)
{
	int ret;
	struct redfish_file *zofe = NULL;
	if (bufsz == 0)
		bufsz = cli->default_bufsz;
	if (repl == 0)
		repl = cli->default_repl;
	if (blocksz == 0)
		blocksz = cli->default_blocksz;
	zofe = calloc(1, sizeof(struct redfish_file));
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

int redfish_open(struct redfish_client *cli, const char *path,
		struct redfish_file **ofe)
{
	int ret;
	struct redfish_file *zofe = calloc(1, sizeof(struct redfish_file));
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
	if (mode != REDFISH_INVAL_MODE)
		hdfsChmod(cli->fs, path, mode);
	return ret;

error:
	if (zofe) {
		free(zofe->path);
		free(zofe);
	}
	return ret;
}

int redfish_mkdirs(struct redfish_client *cli, int mode, const char *path)
{
	char *str, full[PATH_MAX], tbuf[PATH_MAX];
	int ret;

	ret = hdfsCreateDirectory(cli->fs, path);
	if (ret)
		return -EIO;
	if (mode == REDFISH_INVAL_MODE)
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

int redfish_get_block_locs(struct redfish_client *cli, const char *path,
	int64_t start, int64_t len, char ***blc)
{
	int i, nz, na;
	char ***s, ***str = NULL;
	struct redfish_block_loc **zblc = NULL;

	str = hdfsGetHosts(cli->fs, path, start, len);
	if (!str) {
		ret = -errno;
		goto error;
	}
	nz = 0;
	for (s = str; *s; ++s) {
		nz++;
	}
	zblc = calloc(1, sizeof(struct redfish_block_loc*) * (nz + 1));
	if (!zblc) {
		ret = -ENOMEM;
		goto error;
	}
	for (i = 0; i < nz; ++i) {
		char **a;
		int j, na;

		na = 0;
		for (a = *str; *a; ++a) {
			na++;
		}
		zblc[i] = calloc(sizeof(struct redfish_block_loc) + 
				(sizeof(struct(redfish_block_host)) * na));
		// FIXME: the libhdfs API doesn't give us enough information to
		// fill start and len out correctly.
		zblc[i]->start = start;
		zblc[i]->len = len;
		zblc[i]->num_hosts = na;
		for (j = 0; j < na; ++j) {
			struct redfish_block_host *host = &zblc[i]->hosts[j];
			host->hostname = strdup((*str)[j]);
			if (!host->hostname) {
				ret = -ENOMEM;
				goto error;
			}
			// FIXME: the libhdfs API doesn't give us enough
			// information to fill out port correctly.
			host->port = 0;
		}
	}
	*blc = zblc;
	hdfsFreeHosts(str);
	return 0;

error:
	if (str)
		hdfsFreeHosts(str);
	if (zblc) {
		redfish_free_block_locs(zblc);
	}
	return ret;
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

	if ((!osa->path) || (!osa->owner) || (!osa->group)) {
		free(osa->path);
		osa->path = NULL;
		free(osa->owner);
		osa->owner = NULL;
		free(osa->group);
		osa->group = NULL;
		return -ENOMEM;
	}
	return 0;
}

int redfish_get_path_status(struct redfish_client *cli, const char *path,
			      struct offile_status* osa)
{
	int ret;
	hdfsFileInfo *hi
		
	hi = hdfsGetPathInfo(cli->fs, path);
	if (!hi) {
		ret = -ENOENT;
		goto done;
	}
	ret = hdfs_status_to_of_status(hi, osa);
	hdfsFreeFileInfo(hi, 1);
done:
	return ret;
}

int redfish_list_directory(struct redfish_client *cli, const char *dir,
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
		redfish_free_statuses(zosa, nosa);
	return ret;
}

void redfish_free_path_statuses(struct of_status *osa, int nosa)
{
	for (i = 0; i < nosa; ++i) {
		free(osa->path);
		free(osa->owner);
		free(osa->group);
	}
	free(osa);
}

int redfish_chmod(struct redfish_client *cli, const char *path, int mode)
{
	int ret = hdfsChmod(cli->fs, path, mode);
	if (ret)
		return -EIO;
	return 0;
}

int redfish_chown(struct redfish_client *cli, const char *path,
		  const char *owner, const char *group)
{
	int ret = hdfsChown(cli->fs, path, owner, group);
	if (ret)
		return -EIO;
	return 0;
}

int redfish_set_times(struct redfish_client *cli, const char *path,
		      int64_t mtime, int64_t atime)
{
	int ret = hdfsUtime(cli->fs, path, mtime, atime)
	if (ret)
		return -EIO;
	return 0;
}

void redfish_disconnect(struct redfish_client *cli)
{
	free(cli->user);
	hdfsDisconnect(cli->fs);
}

int redfish_read(struct redfish_file *ofe, void *data, int len)
{
	tSize res = hdfsRead(ofe->cli->fs, ofe->file, data, len);
	if (res < 0) {
		return -EIO;
	}
	return (int)res;
}

int32_t redfish_available(struct redfish_file *ofe)
{
	return hdfsAvailable(ofe->cli->fs, ofe->file);
}

int redfish_pread(struct redfish_file *ofe, void *data, int len, int64_t off)
{
	tSize res = hdfsRead(ofe->cli->fs, ofe->file, off, data, len);
	if (res < 0) {
		return -EIO;
	}
	return (int)res;
}

int redfish_write(struct redfish_file *ofe, const void *data, int len)
{
	tSize res = hdfsWrite(ofe->cli->fs, ofe->file, data, len);
	if (res < 0) {
		return -EIO;
	}
	return (int)res;
}

int redfish_fseek(struct redfish_file *ofe, int64_t off)
{
	int res = hdfsSeek(ofe->cli->fs, ofe->file, off);
	if (res < 0) {
		return -EIO;
	}
	return 0;
}

int64_t redfish_ftell(struct redfish_file *ofe)
{
	tOffset res = hdfsTell(ofe->cli->fs, ofe->file);
	if (res < 0) {
		return -EIO;
	}
	return (int64_t)res;
}

int redfish_flush(struct redfish_file *ofe)
{
	int ret = hdfsFlush(ofe->cli->fs, ofe->file)
	if (ret)
		return -EIO;
	return 0;
}

int redfish_sync(struct redfish_file *ofe)
{
	return 0;
}

void redfish_free_file(struct redfish_file *ofe)
{
	if (ofe->cli)
		hdfsCloseFile(ofe->cli->fs, ofe->file);
	ofe->file = NULL;
	free(ofe->path);
	ofe->path = NULL;
	free(ofe);
}

/* FIXME: libhdfs only exposes one delete method... I guess it must delete recursively,
 * because otherwise there would be no way to delete directories. It's not
 * documented anywhere.
 *
 * We probably could/should emulate non-recursive delete somehow here, although
 * it's not going to be properly atomic :(
 */
int redfish_unlink(struct redfish_client *cli, const char *path)
{
	int ret = hdfs_delete(cli->fs, path);
	if (ret)
		return -EIO;
	return 0;
}

int redfish_unlink_tree(struct redfish_client *cli, const char *path)
{
	int ret = hdfs_delete(cli->fs, path);
	if (ret)
		return -EIO;
	return 0;
}

int redfish_rename(struct redfish_client *cli, const char *src, const char *dst)
{
	int ret = hdfsRename(cli->fs, src, dst);
	if (ret)
		return -EIO;
	return 0;
}

int redfish_close(struct redfish_file *ofe)
{
	int ret = hdfsCloseFile(ofe->cli->fs, ofe->file);
	ofe->cli = NULL;
	redfish_free_file(ofe);
	return ret;
}
