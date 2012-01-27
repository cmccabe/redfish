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

#ifndef REDFISH_CLIENT_FISHC_DOT_H
#define REDFISH_CLIENT_FISHC_DOT_H

#include <stdint.h> /* for int64_t */

#include <unistd.h> /* for size_t */

#define REDFISH_INVAL_MODE 01000

/** Represents a Redfish client. Generally you will have one of these for each
 * process that is accessing the filesystem.
 *
 * All operations on a Redfish Client are thread-safe, except for
 * redfish_release_client and redfish_disconnect_and_release.
 */
struct redfish_client;

struct redfish_version
{
	uint32_t major;
	uint32_t minor;
	uint32_t patchlevel;
};

/** Represents the location of a metadata server */
struct redfish_mds_locator
{
	char *host;
	int port;
};

/** Represents an open redfish file.
 *
 * All operations on a Redfish File are thread-safe, except for
 * redfish_free_file and redfish_close_and_free.
 */
struct redfish_file;

/** Represents the status of a Redfish file. */
struct redfish_stat
{
	char *path;
	int64_t length;
	short is_dir;
	short repl;
	int block_sz;
	int64_t mtime;
	int64_t atime;
	short mode;
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
	int nhosts;
	struct redfish_block_host hosts[0];
};

/** Get the version of the redfish client library
 *
 * @return		The redfish version
 */
struct redfish_version redfish_get_version(void);

/** Get the current redfish version as a string
 *
 * @return		a statically allocated string
 */
const char* redfish_get_version_str(void);

/** Create a new Redfish filesystem
 *
 * We will begin by querying the supplied list of hosts, asking each one for a
 * current shard map to get us started.
 *
 * @param uconf		Local path to a Redfish configuration file
 * @param mid		The current metadata server ID
 * @param fsid		The filesystem ID of the new filesystem to create
 * @param err		(out param) error buffer.  If this is set, there was an
 *			error and mkfs failed.
 * @param err_len	Length of the error buffer
 */
void redfish_mkfs(const char *uconf, uint16_t mid, uint64_t fsid,
			char *err, size_t err_len);

/** Initialize a Redfish client instance.
 *
 * We will begin by querying the hosts specified in the configuration file,
 * asking each one for a current shard map to get us started.
 *
 * @param conf_path	Path to a Redfish configuration file
 * @param user		The user to connect as
 * @param cli		(out-parameter): the new redfish_client instance.
 *
 * @return		0 on success; error code otherwise
 *			On success, *cli will contain a valid redfish client.
 */
int redfish_connect(const char *conf_path, const char *user,
			struct redfish_client **cli);

/** Create a new redfish user
 *
 * @param cli		The Redfish client
 * @param user		The new user name
 * @param group		The primary group of the new user
 *
 * @return		0 on success; return code otherwise
 */
int redfish_useradd(struct redfish_client *cli, const char *user,
		const char *group);

/** Delete a redfish user
 * Files and directories assigned by this user will be reassigned to the user
 * 'nobody'.
 *
 * @param cli		The Redfish client
 * @param user		The user name
 *
 * @return		0 on success; return code otherwise
 */
int redfish_userdel(struct redfish_client *cli, const char *user);

/** Create a new redfish group
 *
 * @param cli		The Redfish client
 * @param group		The new group name
 *
 * @return		0 on success; return code otherwise
 */
int redfish_groupadd(struct redfish_client *cli, const char *group);

/** Destroy a redfish group
 * Files and directories assigned to this group will be reassigned to the group
 * 'everyone'.  Users who have this group as their primary group will instead
 * have the primary group 'everyone.'
 *
 * @param cli		The Redfish client
 * @param group		The group name
 *
 * @return		0 on success; return code otherwise
 */
int redfish_groupdel(struct redfish_client *cli, const char *group);

/** Create a file in Redfish
 *
 * @param cli		The Redfish client
 * @param path		The file path
 * @param mode		The file mode
 * @param bufsz		The buffer size to use for the new file.
 *			0 means to use the default buffer size
 * @param repl		The number of replicas to use.
 *			0 means to use the default number of replicas
 * @param blocksz	The size of the blocks to use
 *			0 means to use the default block size
 * @param ofe		(out-parameter) the Redfish file
 *
 * @return		0 on success; error code otherwise
 *			On success, *ofe will contain a valid redfish file.
 */
int redfish_create(struct redfish_client *cli, const char *path,
	int mode, int bufsz, int repl, int blocksz, struct redfish_file **ofe);

/** Open a Redfish file for reading
 *
 * TODO: make buffer size configurable here?
 *
 * @param cli		The Redfish client
 * @param path		The file path
 * @param ofe		(out-parameter) the Redfish file
 *
 * @return		0 on success; error code otherwise
 *			On success, *ofe will contain a valid redfish file.
 */
int redfish_open(struct redfish_client *cli, const char *path,
		struct redfish_file **ofe);

/** Create a regular directory or directories in Redfish.
 *
 * Similar to mkdir -p, we will create the relevant directory as well as any
 * ancestor directories.
 *
 * @param cli		the Redfish client to use
 * @param mode		The permission to use when creating the directories.
 *			If this is REDFISH_INVAL_MODE, the default mode will
 *			be used.
 * @param ofe		(out-parameter): the redfish file
 *
 * Returns: 0 on success; error code otherwise
 * On success, *cli will contain a valid redfish file.
 */
int redfish_mkdirs(struct redfish_client *cli, int mode, const char *path);

/** Get the block locations
 *
 * Get the block locations where a given file is being stored.
 *
 * @param cli		the Redfish client
 * @paths path		path
 * @param start		Start location in the file
 * @param len		Length of the region of the file to examine
 * @param blc		(out-parameter) will contain a NULL-terminated array of
 *			pointers to block locations on success.
 * @return		negative number on error; 0 on success
 */
int redfish_locate(struct redfish_client *cli, const char *path,
	int64_t start, int64_t len, struct redfish_block_loc ***blc);

/** Free the array of block locations
 *
 * @param blc		The array of block locations
 * @param nblc		Length of the array of block locations
 */
void redfish_free_block_locs(struct redfish_block_loc **blc, int nblc);

/** Given a path, returns file status information
 *
 * @param cli		the Redfish client
 * @paths path		path
 * @param osa		(out-parameter): file status
 *
 * @return		0 on success; error code otherwise
 */
int redfish_get_path_status(struct redfish_client *cli, const char *path,
				struct redfish_stat* osa);

/** Frees the status data returned by redfish_get_path_status
 *
 * @param osa		The file status
 */
void redfish_free_path_status(struct redfish_stat* osa);

/** Given a directory name, return a list of status objects corresponding
 * to the objects in that directory.
 *
 * @param cli		the Redfish client
 * @param dir		the directory to get a listing from
 * @param osa		(out-parameter) an array of statuses
 *
 * @return		the number of status objects on success; a negative
 *			error code otherwise
 */
int redfish_list_directory(struct redfish_client *cli, const char *dir,
			      struct redfish_stat** osa);

/** Frees the array of status data returned by redfish_list_directory
 *
 * @param osa		array of file statuses
 * @param nosa		Length of osa
 */
void redfish_free_path_statuses(struct redfish_stat* osa, int nosa);

/** Changes the permission bits for a file or directory.
 *
 * @param cli		the Redfish client
 * @param path		the path
 * @param mode		the new permission bits
 *
 * @return		0 on success; error code otherwise
 */
int redfish_chmod(struct redfish_client *cli, const char *path, int mode);

/** Changes the owner and group of a file or directory.
 *
 * @param cli		the Redfish client
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
 * @param cli		the Redfish client
 * @param path		the path
 * @param mtime		the new mtime, or -1 if the time should not be changed.
 * @param atime		the new atime, or -1 if the time should not be changed.
 *
 * @return		0 on success; error code otherwise
 */
int redfish_utimes(struct redfish_client *cli, const char *path,
		int64_t mtime, int64_t atime);

/** Disconnect a Redfish client instance
 *
 * Once a client instance is disconnected, no further operations can be
 * performed on it.  This function is thread-safe.
 *
 * There isn't anything you can do with a disconnected client except release it
 * using redfish_release_client.
 *
 * @param cli		the Redfish client to disconnect
 */
void redfish_disconnect(struct redfish_client *cli);

/** Release the memory associated with a Redfish client
 *
 * This function will release the memory associated with a Redfish client.
 * The memory may not actually be freed until all of the redfish_file
 * structures created by this client have been freed with redfish_free_file.
 *
 * You must ensure that no other thread is using the Redfish client pointer
 * while you are releasing it, or afterwards.
 *
 * The pointers to redfish_file structures created by this client will continue
 * to be valid even after the client itself is released.  The only really useful
 * thing you can do with those pointers at that time is to call
 * redfish_free_file on them.  Please remember to do this.
 *
 * @param cli		the Redfish client to free
 */
void redfish_release_client(struct redfish_client *cli);

/** Disconnect and free a Redfish client.
 *
 * This function is NOT thread-safe.  See redfish_release_client for an
 * explanation.
 *
 * @param cli		the Redfish client to disconnect and free.
 */
void redfish_disconnect_and_release(struct redfish_client *cli);

/** Reads data from a redfish file
 *
 * @param ofe		the Redfish file
 * @param data		a buffer to read into
 * @param len		the maximum length of the data to read
 *
 * @return		the number of bytes read on success; a negative error
 *			code on failure.  0 indicates EOF.
 *			We won't return a short read unless the file itself is
 *			shorter than the requested length.
 */
int redfish_read(struct redfish_file *ofe, void *data, int len);

/** Returns the number of bytes that can be read from the Redfish file without
 * blocking.
 *
 * @param ofe		the Redfish file
 *
 * @return		the number of bytes available to read
 */
int32_t redfish_available(struct redfish_file *ofe);

/** Reads data from a redfish file
 *
 * @param ofe		the Redfish file
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
 * @param ofe		the Redfish file
 * @param data		the data to write
 * @param len		the length of the data to write
 *
 * @return		0 on success; error code otherwise
 */
int redfish_write(struct redfish_file *ofe, const void *data, int len);

/** Set the current position in a file
 * This works only for files opened in read-only mode.
 *
 * @param ofe		the Redfish file
 * @param off		the desired new offset
 *
 * @return		0 on success; error code otherwise
 */
int redfish_fseek_abs(struct redfish_file *ofe, int64_t off);

/** Set the current position in a file
 * This works only for files opened in read-only mode.
 *
 * @param ofe		the Redfish file
 * @param delta		the desired change in offset
 * @param out		(out param) the actual change in offset that was made
 *
 * @return		0 on success; error code otherwise
 */
int redfish_fseek_rel(struct redfish_file *ofe, int64_t delta, int64_t *out);

/** Get the current position in a file
 *
 * @param ofe		the Redfish file
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

/** Close a Redfish file.
 *
 * For files opened for writing or appending, this triggers any locally
 * buffered data to be written out to the metadata servers.
 *
 * This operation is thread-safe.
 *
 * @param ofe		the Redfish file
 *
 * @return		0 on success; error code if the buffered data could not
 *			be written out as expected.
 */
int redfish_close(struct redfish_file *ofe);

/** Freed the memory and internal state associated with a Redfish file.
 *
 * This operation does NOT properly close the file.  You may lose data if you
 * free a file opened for write before closing it.
 *
 * This operation is NOT thread-safe!  You must ensure that no other thread is
 * using the Redfish file while it is being freed.  After the file is freed, the
 * pointer becomes invalid and must never be used again.
 *
 * @param ofe		the Redfish file
 */
void redfish_free_file(struct redfish_file *ofe);

/** Close and free a Redfish file.
 *
 * This is a convenience method.  It is equivalent to redfish_close followed by
 * redfish_free_file.  Like redfish_free_file, it is NOT thread-safe.
 *
 * @param ofe		the Redfish file
 *
 * @return		the return code of redfish_close
 */
int redfish_close_and_free(struct redfish_file *ofe);

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
