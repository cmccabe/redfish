/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * * This is licensed under the Apache License, Version 2.0.
 */

#ifndef ONEFISH_CLIENT_DOT_H
#define ONEFISH_CLIENT_DOT_H

#include <stdint.h>

/** Represents a onefish client. Generally you will have one of these for each
 * process that is accessing the filesystem.
 * It can be shared among multiple threads.
 */
struct ofclient;

#define ONEFISH_INVAL_MODE 0xffffu

/** Represents the location of a metadata server */
struct of_mds_locator
{
	const char *host;
	int port;
};

/** Represents an open onefish file. */
struct offile;

/** An iterator for file block locations */
struct of_block_loc_iter;

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
 * mlocs: NULL-terminated list of metadata server locations to connect to
 * user: the user to connect as
 * cli (out-parameter): where to place the new ofclient instance.
 *
 * Returns: 0 on success; error code otherwise
 * On success, *cli will contain a valid onefish client.
 */
int onefish_connect(struct of_mds_locator **mlocs, const char *user,
			struct ofclient **cli);

/** Get the default replication level.
 *
 * cli: the onefish client
 *
 * Returns: the default replication level
 */
int get_default_repl(const struct ofclient *cli);

/** Set the default replication level.
 *
 * cli: the onefish client
 * repl: the new default replication level
 */
void set_default_repl(struct ofclient *cli, int repl);

/** Open a regular file in OneFish.
 *
 * cli: the OneFish client
 * path: path to the file
 * flags: supported combinations:
 * 	O_RDONLY for reading.
 * 	O_WRONLY | O_CREAT | O_TRUNC to create a new file, overwriting any existing file.
 * 	O_WRONLY | O_APPEND to open an existing file in append mode.
 * mode: the permission to use when creating this file (ignored for O_RDONLY)
 * bufsz: the size of the buffer to use, or 0 to use the default.
 * repl: the replication count, or 0 to use the default.
 * ofe (out-parameter): the onefile file
 *
 * Returns: 0 on success; error code otherwise
 * On success, *cli will contain a valid onefish file.
 */
int onefish_open(struct ofclient *cli, const char *path,
		int flags, int mode, int bufsz, int repl,
		struct offile **ofe);

/** Set the replication count on a file in OneFish
 *
 * cli: the OneFish client
 * path: the path
 * repl: the new replication count
 *
 * Returns: 0 on success; error code otherwise
 */
int onefish_set_replication(struct ofclient *cli, const char *path, int repl);

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
int onefish_mkdirs(struct ofclient *cli, const char *path);

/** Get the block locations
 *
 * Get the block locations where a given file is being stored. For every call
 * to onefish_get_block_locs_start, there must be a corresponding call to
 * onefish_get_block_locs_end
 *
 * cli: the OneFish client to use
 * mode: the permission to use when opening this file.
 * ofe (out-parameter): the onefile file
 *
 * Returns: 0 on success; error code otherwise
 * On success, *biter will contain a valid onefish block locations iterator.
 */
int onefish_get_block_locs_start(struct offile *ofe, uint64_t start, uint64_t len,
					struct of_block_loc_iter **biter);

/** Given a block location iterator, get the next block location
 *
 * biter: The block location iterator to use
 *
 * Returns: the next block location, or NULL if there are no more block
 * locations
 */
struct of_block_loc* onefish_get_block_locs_next(struct of_block_loc_iter *biter);

/** Given a block location iterator, close the iterator and free any associated
 * resources and memory.
 *
 * biter: The block location iterator to use
 */
void onefish_get_block_locs_end(struct of_block_loc_iter *biter);

/** Given a list of paths, returns a list of file status objects corresponding
 * to those pathers (excluding non-existent paths).
 *
 * paths: NULL-terminated array of pointers to path strings
 * You are responsible for allocating and freeing these paths.
 *
 * cli: the OneFish client
 * osa (out-parameter): NULL-terminated array of file statuses
 * Free these with onefish_free_file_statuses.
 */
int onefish_get_file_statuses(struct ofclient *cli, const char **paths,
			      struct offile_status** osa);

/** Given a directory name, return a list of file status objects corresponding
 * to the objects in that directory.
 *
 * cli: the OneFish client
 * dir: the directory to get a listing from
 * osa (out-parameter): NULL-terminated array of file statuses
 * Free these with onefish_free_file_statuses.
 */
int onefish_list_directory(struct ofclient *cli, const char *dir,
			      struct offile_status** osa);

/** Frees the file status data returned by onefish_get_file_statuses
 *
 * osa: NULL-terminated array of file statuses
 */
void onefish_free_file_statuses(struct offile_status* osa);

/** Changes the permission bits for a file or directory.
 *
 * cli: the OneFish client
 * path: the path
 * mode: the new permission bits
 *
 * Returns 0 on success; error code otherwise.
 */
int onefish_set_permission(struct ofclient *cli, const char *path, int mode);

/** Changes the owner for a file or directory.
 *
 * cli: the OneFish client
 * path: the path
 * mode: the new owner name
 *
 * Returns 0 on success; error code otherwise.
 */
int onefish_set_owner(struct ofclient *cli, const char *path, const char *owner);

/** Changes the mtime and atime of a file
 *
 * cli: the OneFish client
 * path: the path
 * mtime: the new mtime, or -1 if the time should not be changed.
 * atime: the new atime, or -1 if the time should not be changed.
 *
 * Returns 0 on success; error code otherwise.
 */
int onefish_set_times(struct ofclient *cli, const char *path,
		      int64_t mtime, int64_t atime);

/** Destroy a OneFish client instance and free the memory associated with it.
 *
 * cli: the OneFish client to destroy
 */
void onefish_disconnect(struct ofclient *cli);

/** Reads data from a onefish file
 *
 * ofe: the OneFish file
 * data: a buffer to read into
 * len: the maximum length of the data to read
 *
 * Returns the number of bytes read on success; a negative error code
 * on failure.
 */
int onefish_read(struct offile *ofe, uint8_t *data, int len);

/** Writes data to a onefish file
 *
 * ofe: the OneFish file
 * data: the data to write
 * len: the length of the data to write
 *
 * Returns 0 on success; error code otherwise
 */
int onefish_write(struct offile *ofe, const uint8_t *data, int len);

/** Set the current position in a file
 * This works only for files opened in read-only mode.
 *
 * ofe: the OneFish file
 *
 * Returns 0 on success; error code otherwise
 */
int onefish_fseek(struct offile *ofe, uint64_t off);

/** Get the current position in a file
 * question: what is this supposed to do for files opened with append? who
 * knows.
 *
 * ofe: the OneFish file
 *
 * Returns the current position in the file
 */
uint64_t onefish_ftell(struct offile *ofe);

/** Persist block locations on the metadata servers, and send all outstanding
 * data to all datanodes in the pipeline.
 *
 * ofe: The onefish file to flush
 *
 * Returns 0 on success; error code otherwise
 */
int onefish_flush(struct offile *ofe);

/** Block until the data is really written to disk on by the chunkservers.
 *
 * ofe: The onefish file to sync
 *
 * Returns 0 on success; error code otherwise
 */
int onefish_sync(struct offile *ofe);

/** Freed the memory and internal state associated with a onefish file.
 *
 * You usually do not want to call this function directly, except when handling
 * errors. It is usually easier to call onefish_close.
 *
 * ofe: the OneFish file
 */
void onefish_free_file(struct offile *ofe);

/** Delete a file or directory
 *
 * cli: the onefish client
 * path: the path to delete
 *
 * returns 0 on success; error code otherwise
 */
int onefish_delete(struct ofclient *cli, const char *path);

/** Rename a file or directory
 *
 * src: the source path
 * dst: the destination path
 *
 * returns 0 on success; error code otherwise
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
 * ofe: the OneFish file
 *
 * Returns 0 on success; error code if onefish_flush failed.
 * Either way, the offile is freed.
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
