/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * * This is licensed under the Apache License, Version 2.0.
 */

#ifndef REDFISH_CLIENT_FISHC_DOT_H
#define REDFISH_CLIENT_FISHC_DOT_H

#include <stdint.h> /* for int64_t */

#include <unistd.h> /* for size_t */

#define REDFISH_INVAL_MODE 01000

/** Represents a redfish client. Generally you will have one of these for each
 * process that is accessing the filesystem.
 * It can be shared among multiple threads.
 */
struct redfish_client;

/** Represents the location of a metadata server */
struct redfish_mds_locator
{
	char *host;
	int port;
};

/** Represents an open redfish file. */
struct redfish_file;

/** Represents the status of a RedFish file. */
struct redfish_stat
{
	char *path;
	int64_t length;
	short is_dir;
	short repl;
	int block_sz;
	int64_t mtime;
	int64_t atime;
	int mode;
	char *owner;
	char *group;
};

struct redfish_block_host
{
	int port;
	char *hostname;
};

struct redfish_block_loc
{
	int64_t start;
	int64_t len;
	int num_hosts;
	struct redfish_block_host hosts[0];
};

/** Initialize a RedFish client instance.
 *
 * We will begin by querying the supplied list of hosts, asking each one for a
 * current shard map to get us started.
 *
 * @param mlocs		A NULL-terminated list of metadata server locations to
 *			connect to
 * @param user		The user to connect as
 * @param cli		(out-parameter): the new redfish_client instance.
 *
 * @return		0 on success; error code otherwise
 *			On success, *cli will contain a valid redfish client.
 */
int redfish_connect(struct redfish_mds_locator **mlocs, const char *user,
			struct redfish_client **cli);

/** Apend an entry to a dynamically allocated list of mlocs
 *
 * @param mlocs		The list of mlocs
 * @param s		The entry to append, a C string in host:port format
 * @param err		(out param) error buffer
 * @param err_len	length of err buffer
 */
extern void redfish_mlocs_append(struct redfish_mds_locator ***mlocs, const char *s,
				char *err, size_t err_len);

/** Create a file in RedFish
 *
 * @param cli		The RedFish client
 * @param path		The file path
 * @param mode		The file mode
 * @param bufsz		The buffer size to use for the new file.
 *			0 means to use the default buffer size
 * @param repl		The number of replicas to use.
 *			0 means to use the default number of replicas
 * @param blocksz	The size of the blocks to use
 *			0 means to use the default block size
 * @param ofe		(out-parameter) the RedFish file
 *
 * @return		0 on success; error code otherwise
 *			On success, *ofe will contain a valid redfish file.
 */
int redfish_create(struct redfish_client *cli, const char *path,
	int mode, int bufsz, int repl, int blocksz, struct redfish_file **ofe);

/** Open a RedFish file for reading
 *
 * @param cli		The RedFish client
 * @param path		The file path
 * @param ofe		(out-parameter) the RedFish file
 *
 * @return		0 on success; error code otherwise
 *			On success, *ofe will contain a valid redfish file.
 */
int redfish_open(struct redfish_client *cli, const char *path,
		struct redfish_file **ofe);

/** Create a regular directory or directories in RedFish.
 *
 * Similar to mkdir -p, we will create the relevant directory as well as any
 * ancestor directories.
 *
 * @param cli		the RedFish client to use
 * @param mode		The permission to use when creating the directories.
 *			If this is REDFISH_INVAL_MODE, the default mode will
 *			be used.
 * @param ofe		(out-parameter): the redfish file
 *
 * Returns: 0 on success; error code otherwise
 * On success, *cli will contain a valid redfish file.
 */
int redfish_mkdirs(struct redfish_client *cli, const char *path, int mode);

/** Get the block locations
 *
 * Get the block locations where a given file is being stored.
 *
 * @param cli		the RedFish client
 * @paths path		path
 * @param start		Start location in the file
 * @param len		Length of the region of the file to examine
 * @param blc		(out-parameter) will contain a NULL-terminated array of
 *			pointers to block locations on success.
 * @return		negative number on error; 0 on success
 */
int redfish_get_block_locs(struct redfish_client *cli, const char *path,
	int64_t start, int64_t len, struct redfish_block_loc ***blc);

/** Free the array of block locations
 *
 * @param blc		The array of block locations
 */
void redfish_free_block_locs(struct redfish_block_loc **blc);

/** Given a path, returns file status information
 *
 * @param cli		the RedFish client
 * @paths path		path
 * @param osa		(out-parameter): file status
 *
 * @return		0 on success; error code otherwise
 */
int redfish_get_path_status(struct redfish_client *cli, const char *path,
				struct redfish_stat* osa);

/** Given a directory name, return a list of status objects corresponding
 * to the objects in that directory.
 *
 * @param cli		the RedFish client
 * @param dir		the directory to get a listing from
 * @param osa		(out-parameter) NULL-terminated array of statuses
 *
 * @return		0 on success; error code otherwise
 */
int redfish_list_directory(struct redfish_client *cli, const char *dir,
			      struct redfish_stat** osa);

/** Frees the status data returned by redfish_list_directory
 *
 * @param osa		array of file statuses
 * @param nosa		Length of osa
 */
void redfish_free_path_statuses(struct redfish_stat* osa, int nosa);

/** Changes the permission bits for a file or directory.
 *
 * @param cli		the RedFish client
 * @param path		the path
 * @param mode		the new permission bits
 *
 * @return		0 on success; error code otherwise
 */
int redfish_chmod(struct redfish_client *cli, const char *path, int mode);

/** Changes the owner and group of a file or directory.
 *
 * @param cli		the RedFish client
 * @param path		the path
 * @param owner		the new owner name, or NULL to leave owner unchanged
 * @param group		the new group name, or NULL to leave group unchanged
 *
 * @return		0 on success; error code otherwise
 */
int redfish_chown(struct redfish_client *cli, const char *path,
		  const char *owner, const char *group);

/** Changes the mtime and atime of a file
 *
 * @param cli		the RedFish client
 * @param path		the path
 * @param mtime		the new mtime, or -1 if the time should not be changed.
 * @param atime		the new atime, or -1 if the time should not be changed.
 *
 * @return		0 on success; error code otherwise
 */
int redfish_utimes(struct redfish_client *cli, const char *path,
		int64_t mtime, int64_t atime);

/** Destroy a RedFish client instance and free the memory associated with it.
 *
 * @param cli		the RedFish client to destroy
 */
void redfish_disconnect(struct redfish_client *cli);

/** Reads data from a redfish file
 *
 * @param ofe		the RedFish file
 * @param data		a buffer to read into
 * @param len		the maximum length of the data to read
 *
 * @return		the number of bytes read on success; a negative error
 *			code on failure.
 */
int redfish_read(struct redfish_file *ofe, void *data, int len);

/** Returns the number of bytes that can be read from the RedFish file without
 * blocking.
 *
 * @param ofe		the RedFish file
 *
 * @return		the number of bytes available to read
 */
int32_t redfish_available(struct redfish_file *ofe);

/** Reads data from a redfish file
 *
 * @param ofe		the RedFish file
 * @param data		a buffer to read into
 * @param len		the maximum length of the data to read
 * @param off		offset to read data from
 *
 * @return		the number of bytes read on success; a negative error
 *			code on failure.
 */
int redfish_pread(struct redfish_file *ofe, void *data, int len, int64_t off);

/** Writes data to a redfish file
 *
 * @param ofe		the RedFish file
 * @param data		the data to write
 * @param len		the length of the data to write
 *
 * @return		0 on success; error code otherwise
 */
int redfish_write(struct redfish_file *ofe, const void *data, int len);

/** Set the current position in a file
 * This works only for files opened in read-only mode.
 *
 * @param ofe		the RedFish file
 * @param off		the desired new offset
 *
 * @return		0 on success; error code otherwise
 */
int redfish_fseek_abs(struct redfish_file *ofe, int64_t off);

/** Set the current position in a file
 * This works only for files opened in read-only mode.
 *
 * @param ofe		the RedFish file
 * @param delta		the desired change in offset
 * @param out		(out param) the actual change in offset that was made
 *
 * @return		0 on success; error code otherwise
 */
int redfish_fseek_rel(struct redfish_file *ofe, int64_t delta, int64_t *out);

/** Get the current position in a file
 *
 * @param ofe		the RedFish file
 *
 * Returns the current position in the file
 */
int64_t redfish_ftell(struct redfish_file *ofe);

/** Make all the data in this file visible to readers who open the file after
 * this call to hflush. Data will be visible eventually to readers who already
 * have the file open, but do not reopen it. 
 *
 * This is a blocking call. It will start the hflush operation and then block
 * until it completes. Among other things, this implies that the data is
 * replicated on all relevant OSDs.
 *
 * This call can be used only on files that have been opened in append mode.
 *
 * @param ofe		The redfish file
 *
 * @return		0 on success; error code otherwise
 */
int redfish_hflush(struct redfish_file *ofe);

/** Block until the data is really written to disk on by the chunkservers.
 *
 * This will actually block until the data has been persisted to disk.
 * Basically, fsync.
 *
 * @param ofe		The redfish file to sync
 *
 * @return		0 on success; error code otherwise
 */
int redfish_hsync(struct redfish_file *ofe);

/** Freed the memory and internal state associated with a redfish file.
 *
 * You usually do not want to call this function directly, except when handling
 * errors. It is usually easier to call redfish_close.
 *
 * @param ofe		the RedFish file
 */
void redfish_free_file(struct redfish_file *ofe);

/** Delete a file
 *
 * @param cli		the redfish client
 * @param path		the file to unlink
 *
 * @return		0 on success; error code otherwise
 */
int redfish_unlink(struct redfish_client *cli, const char *path);

/** Delete a file of directory subtree
 *
 * @param cli		the redfish client
 * @param path		the file or subtree to remove
 *
 * @return		0 on success; error code otherwise
 */
int redfish_unlink_tree(struct redfish_client *cli, const char *path);

/** Rename a file or directory
 *
 * @param src		the source path
 * @param dst		the destination path
 *
 * @return		0 on success; error code otherwise
 */
int redfish_rename(struct redfish_client *cli, const char *src, const char *dst);

/** Closes a RedFish file.
 * For files opened for writing or appending, this triggers any locally
 * buffered data to be written out to the metadata servers. Basically, this
 * call is equivalent to redfish_flush followed by redfish_free_file.
 *
 * For files opened for reading, this call is identical to
 * redfish_free_file.
 *
 * @param ofe		the RedFish file
 *
 * @return		0 on success; error code if redfish_flush failed.
 *			Either way, the redfish_file is freed.
 */
int redfish_close(struct redfish_file *ofe);

/* TODO: add copy_from_local_file
 * TODO: add move_from_local_file
 * TODO: add copy_to_local_file
 * TODO: add move_to_local_file
 * TODO: implement checksumming...
 * TODO: implement getcwd / setcwd
 * TODO: implement get capacity / get used
 * TODO: implement list_dir with filtering?
 * TODO: add block size changing functionality
int redfish_set_replication(struct redfish_client *cli, const char *path, int repl)
 * TODO: implement append / hflush / hsync
 */

#endif
