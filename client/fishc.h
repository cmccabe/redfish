/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * * This is licensed under the Apache License, Version 2.0.
 */

#ifndef ONEFISH_CLIENT_FISHC_DOT_H
#define ONEFISH_CLIENT_FISHC_DOT_H

#include <stdint.h>

/** Represents a onefish client. Generally you will have one of these for each
 * process that is accessing the filesystem.
 * It can be shared among multiple threads.
 */
struct ofclient;

/** Represents the location of a metadata server */
struct of_mds_locator
{
	const char *host;
	int port;
};

/** Represents an open onefish file. */
struct offile;

/** Represents the location of a block */
struct of_block_loc
{
	/** The hostname of the datanode with the block */
	const char *host;

	/** The port number of the datanode with the block */
	int port;
};

/** Represents the status of a OneFish file. */
struct offile_status
{
	char *path;
	uint64_t length;
	short is_dir;
	short repl;
	int block_sz;
	int64_t mtime;
	int64_t atime;
	int mode;
	char *owner;
	char *group;
};

/** Initialize a OneFish client instance.
 *
 * We will begin by querying the supplied list of hosts, asking each one for a
 * current shard map to get us started.
 *
 * @param mlocs		A NULL-terminated list of metadata server locations to 
 *			connect to
 * @param user		The user to connect as
 * @param cli		(out-parameter): the new ofclient instance.
 *
 * @return		0 on success; error code otherwise
 *			On success, *cli will contain a valid onefish client.
 */
int onefish_connect(struct of_mds_locator **mlocs, const char *user,
			struct ofclient **cli);

/** Create a file in OneFish
 *
 * @param cli		The OneFish client
 * @param path		The file path
 * @param mode		The file mode
 * @param bufsz		The buffer size to use for the new file.
 *			0 means to use the default buffer size
 * @param repl		The number of replicas to use.
 *			0 means to use the default number of replicas
 * @param ofe		(out-parameter) the OneFish file
 *
 * @return		0 on success; error code otherwise
 *			On success, *ofe will contain a valid onefish file.
 */
int onefish_create(struct ofclient *cli, const char *path,
		int mode, int bufsz, int repl, struct offile **ofe);

/** Open a OneFish file for reading
 *
 * @param cli		The OneFish client
 * @param path		The file path
 * @param ofe		(out-parameter) the OneFish file
 *
 * @return		0 on success; error code otherwise
 *			On success, *ofe will contain a valid onefish file.
 */
int onefish_open(struct ofclient *cli, const char *path,
		struct offile **ofe);

/** Create a regular directory or directories in OneFish.
 *
 * Similar to mkdir -p, we will create the relevant directory as well as any
 * ancestor directories.
 *
 * cli: the OneFish client to use
 * mode: the permission to use when opening this file.
 *	If this is ONEFISH_INVAL_MODE, the default mode will be used.
 * ofe (out-parameter): the onefile file
 *
 * Returns: 0 on success; error code otherwise
 * On success, *cli will contain a valid onefish file.
 */
int onefish_mkdirs(struct ofclient *cli, const char *path, int mode);

/** Get the block locations
 *
 * Get the block locations where a given file is being stored.
 *
 * @param ofe		The onefish file to use
 * @param start		Start location in the file
 * @param len		Length of the region of the file to examine
 * @param blc		(out-parameter) will contain an array of block
 *			locations on success.
 * @return		negative number on error; number of block locations
 * 			on success.
 */
int onefile_get_block_locs(struct offile *ofe,
	uint64_t start, uint64_t len, struct of_block_loc **blc);

/** Free the array of block locations
 *
 * @param blc		The array of block locations
 * @param nblc		Length of blc
 */
void onefile_free_block_locs(struct of_block_loc **blc, int nblc);

/** Given a list of paths, returns a list of file status objects corresponding
 * to those paths (excluding non-existent paths).
 *
 * @param cli		the OneFish client
 * @paths		NULL-terminated array of pointers to path strings
 * @param osa		(out-parameter): array of file statuses
 			Free these with onefish_free_file_statuses.
 *
 * @return		Length of file status array returned, or negative on
 *			error.
 */
int onefish_get_file_statuses(struct ofclient *cli, const char **paths,
			      struct offile_status** osa);

/** Frees the file status data returned by onefish_get_file_statuses
 *
 * @param osa		array of file statuses
 * @param nosa		Length of osa
 */
void onefish_free_file_statuses(struct offile_status* osa, int nosa);

/** Given a directory name, return a list of file status objects corresponding
 * to the objects in that directory.
 *
 * @param cli		the OneFish client
 * @param dir		the directory to get a listing from
 * @param osa		(out-parameter) NULL-terminated array of file statuses
 *
 * @return		0 on success; error code otherwise
 */
int onefish_list_directory(struct ofclient *cli, const char *dir,
			      struct offile_status** osa);

/** Changes the permission bits for a file or directory.
 *
 * @param cli		the OneFish client
 * @param path		the path
 * @param mode		the new permission bits
 *
 * @return		0 on success; error code otherwise
 */
int onefish_set_permission(struct ofclient *cli, const char *path, int mode);

/** Changes the owner of a file or directory.
 *
 * @param cli		the OneFish client
 * @param path		the path
 * @param mode		the new owner name
 *
 * @return		0 on success; error code otherwise
 */
int onefish_set_owner(struct ofclient *cli, const char *path, const char *owner);

/** Changes the mtime and atime of a file
 *
 * @param cli		the OneFish client
 * @param path		the path
 * @param mtime		the new mtime, or -1 if the time should not be changed.
 * @param atime		the new atime, or -1 if the time should not be changed.
 *
 * @return		0 on success; error code otherwise
 */
int onefish_set_times(struct ofclient *cli, const char *path,
		      int64_t mtime, int64_t atime);

/** Destroy a OneFish client instance and free the memory associated with it.
 *
 * @param cli		the OneFish client to destroy
 */
void onefish_disconnect(struct ofclient *cli);

/** Reads data from a onefish file
 *
 * @param ofe		the OneFish file
 * @param data		a buffer to read into
 * @param len		the maximum length of the data to read
 *
 * @return		the number of bytes read on success; a negative error
 *			code on failure.
 */
int onefish_read(struct offile *ofe, uint8_t *data, int len);

/** Writes data to a onefish file
 *
 * @param ofe		the OneFish file
 * @param data		the data to write
 * @param len		the length of the data to write
 *
 * @return		0 on success; error code otherwise
 */
int onefish_write(struct offile *ofe, const uint8_t *data, int len);

/** Set the current position in a file
 * This works only for files opened in read-only mode.
 *
 * @param ofe		the OneFish file
 * @param off		the desired new offset
 *
 * @return		0 on success; error code otherwise
 */
int onefish_fseek(struct offile *ofe, uint64_t off);

/** Get the current position in a file
 *
 * @param ofe		the OneFish file
 *
 * Returns the current position in the file
 */
uint64_t onefish_ftell(struct offile *ofe);

/** Persist block locations on the metadata servers, and send all outstanding
 * data to all datanodes in the pipeline.
 *
 * @param ofe		The onefish file to flush
 *
 * @return		0 on success; error code otherwise
 */
int onefish_flush(struct offile *ofe);

/** Block until the data is really written to disk on by the chunkservers.
 *
 * @param ofe		The onefish file to sync
 *
 * @return		0 on success; error code otherwise
 */
int onefish_sync(struct offile *ofe);

/** Freed the memory and internal state associated with a onefish file.
 *
 * You usually do not want to call this function directly, except when handling
 * errors. It is usually easier to call onefish_close.
 *
 * @param ofe		the OneFish file
 */
void onefish_free_file(struct offile *ofe);

/** Delete a file or directory
 *
 * @param cli		the onefish client
 * @param path		the path to delete
 *
 * @return		0 on success; error code otherwise
 */
int onefish_delete(struct ofclient *cli, const char *path);

/** Rename a file or directory
 *
 * @param src		the source path
 * @param dst		the destination path
 *
 * @return		0 on success; error code otherwise
 */
int onefish_rename(struct ofclient *cli, const char *src, const char *dst);

/** Closes a OneFish file.
 * For files opened for writing or appending, this triggers any locally
 * buffered data to be written out to the metadata servers. Basically, this
 * call is equivalent to onefish_flush followed by onefish_free_file.
 *
 * For files opened for reading, this call is identical to
 * onefish_free_file.
 *
 * @param ofe		the OneFish file
 *
 * @return		0 on success; error code if onefish_flush failed.
 *			Either way, the offile is freed.
 */
int onefish_close(struct offile *ofe);

/* TODO: add copy_from_local_file
 * TODO: add move_from_local_file
 * TODO: add copy_to_local_file
 * TODO: add move_to_local_file
 * TODO: implement checksumming...
 * TODO: implement hdfsAvailable equivalent...
 * TODO: implement getcwd / setcwd
 * TODO: implement get capacity / get used
 * TODO: implement list_dir with filtering
 * TODO: add block size changing functionality
 */

#endif
