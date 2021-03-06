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

#ifndef REDFISH_MDS_MSTOR_DOT_H
#define REDFISH_MDS_MSTOR_DOT_H

#include <stdio.h> /* for FILE */
#include <stdint.h> /* uint32_t, etc. */

#include "mds/const.h"
#include "msg/types.h" /* for RF_MAX_OID */

#define MSTOR_ROOT_NID 1

#define MNODE_IS_DIR 0x8000

/*
 * The metadata storage facility.
 *
 * Some notes:
 * You must canonicalize all paths before giving them to the mstor.  Do not
 * trust the users-- they are evil.
 *
 * You must quiesce all threads before changing the udata structure.  Since new
 * users and groups are added rather infrequently, this should be as acceptable.
 */
struct fast_log_mgr;
struct mstor;
struct srange_locker;
struct udata;

enum mstor_op_ty {
	/** Operation that changes a user's primary group
	 * Locking: uses range locker */
	MSTOR_OP_SET_PRIMARY_USER_GROUP = 1,
	/** Operation that adds a user to a group
	 * Locking: uses range locker */
	MSTOR_OP_ADD_USER_TO_GROUP,
	/** Operation that remove a user from a group
	 * Locking: uses range locker */
	MSTOR_OP_REMOVE_USER_FROM_GROUP,
	/** Operation that creates a file
	 * Locking: uses range locker */
	MSTOR_OP_CREAT,
	/** Operation that opens a file
	 * Locking: uses range locker */
	MSTOR_OP_OPEN,
	/** Operation that locates chunks within a file
	 * Locking: uses range locker */
	MSTOR_OP_CHUNKFIND,
	/** Operation that allocates a new chunk
	 * Locking: not required */
	MSTOR_OP_CHUNKALLOC,
	/** Operation that creates a directory or set of directories
	 * Locking: uses range locker */
	MSTOR_OP_MKDIRS,
	/** Operation that lists entries in a directory
	 * Locking: uses range locker */
	MSTOR_OP_LISTDIR,
	/** Operation that gets information about a file or directory
	 * based on its path.  Locking: uses range locker */
	MSTOR_OP_STAT,
	/** Operation that gets information about a file or directory
	 * based on its node ID.  Locking: none */
	MSTOR_OP_NID_STAT,
	/** Operation that changes the mode
	 * Locking: uses range locker */
	MSTOR_OP_CHMOD,
	/** Operation that changes ownership
	 * Locking: uses range locker */
	MSTOR_OP_CHOWN,
	/** Operation that changes modification and access times.
	 * Locking: uses range locker */
	MSTOR_OP_UTIMES,
	/** Operation that unlinks a file or directory
	 * Locking: uses range locker */
	MSTOR_OP_UNLINK,
	/** Operation that finds zombie chunks.
	 * Locking: external */
	MSTOR_OP_FIND_ZOMBIES,
	/** Operation that destroys a zombie chunk.
	 * Locking: external */
	MSTOR_OP_DESTROY_ZOMBIE,
	/** Operation that renames a directory or file
	 * Locking: uses range locker */
	MSTOR_OP_RENAME,
	/** For mstor internal use only */
	MSTOR_OP_NODE_SEARCH,
};

struct mreq {
	/** (caller sets) String range locker to use.
	 * You must set lk->sem.  The other fields will be overwritten. */
	struct srange_locker *lk;
	/** (caller sets) Operation type */
	enum mstor_op_ty op;
	/** (caller sets) Full path of request */
	const char *full_path;
	/** (caller sets) User performing request */
	const char *user_name;
	/** (internal) user entry */
	struct user *user;
	/** (internal) Flags. */
	int flags;
	/** Operation type-specific data */
	char data[0];
};

struct mreq_set_primary_user_group {
	struct mreq base;
	/** User to modify */
	const char *tgt_user;
	/** New primary group to set */
	const char *tgt_group;
};

struct mreq_add_user_to_group {
	struct mreq base;
	/** User to modify */
	const char *tgt_user;
	/** New group to add */
	const char *tgt_group;
};

struct mreq_remove_user_from_group {
	struct mreq base;
	/** User to modify */
	const char *tgt_user;
	/** Group to remove */
	const char *tgt_group;
};

struct mreq_creat {
	struct mreq base;
	/** Mode to create file with */
	uint16_t mode;
	/** time to create file with */
	uint64_t ctime;
	/** (out param) new node id */
	uint64_t nid;
};

struct mreq_open {
	struct mreq base;
	/** New atime to use */
	uint64_t atime;
	/** (out param) file node ID */
	uint64_t nid;
};

struct chunk_info {
	uint64_t cid;
	uint64_t base;
};

struct mreq_chunkfind {
	struct mreq base;
	/** start of region */
	uint64_t start;
	/** end of region */
	uint64_t end;
	/** number of chunk IDs that can fit in this buffer */
	int max_cinfos;
	/** (out param) number of chunk IDs retrieved */
	int num_cinfos;
	/** (out param) pointer to a buffer where we'll put the retrieved chunk
	 * information */
	struct chunk_info *cinfos;
};

struct mreq_chunkalloc {
	struct mreq base;
	/** Node ID of file */
	uint64_t nid;
	/** Starting offset in the file of the new chunk */
	uint64_t off;
	/** (out-param) new chunk ID */
	uint64_t cid;
	/** (out-param) OSD IDs where the new chunk will be stored */
	uint32_t oid[RF_MAX_OID];
	/** (out-param) length of oid array */
	int num_oid;
};

struct mreq_mkdirs {
	struct mreq base;
	/** Mode to create directory with */
	uint16_t mode;
	/** time to create directory with */
	uint64_t ctime;
};

struct mreq_stat {
	struct mreq base;
	/** (out param) Redfish stat structure.  Must be freed with XDR_FREE
	 * after use. */
	struct rf_stat *stat;
};

struct mreq_nid_stat {
	struct mreq base;
	/** Node ID to look up */
	uint64_t nid;
	/** (out param) Redfish stat structure.  Must be freed with XDR_FREE
	 * after use. */
	struct rf_stat *stat;
};

struct mreq_listdir {
	struct mreq base;
	/** (inout param) Pointer to buffer to use to return the results.
	 * The results will be returned as an array of rf_stat entries. */
	struct rf_lentry *le;
	/** Maximum number of rf_lentry structures that can fit in our
	 * buffer. */
	int max_stat;
	/** (out param) Number of stat structures returned */
	int num_stat;
};

struct mreq_chown {
	struct mreq base;
	/** New owner, or zero-length string for no change */
	const char *new_user;
	/** New group, or zero-length string for no change */
	const char *new_group;
};

struct mreq_chmod {
	struct mreq base;
	/** New mode (type bits are not included) */
	uint16_t mode;
};

struct mreq_utimes {
	struct mreq base;
	/** New access time, or RF_INVAL_TIME if the access time should not
	 * change.  */
	uint64_t new_atime;
	/** New modification time, or RF_INVAL_TIME if the modification time
	 * should not change.  */
	uint64_t new_mtime;
};

struct mreq_unlink {
	struct mreq base;
	/** Time of unlink operation */
	uint64_t ztime;
	/** Unlink operation type
	 */
	enum mmm_unlink_op uop;
};

struct zombie_info {
	/** chunk ID */
	uint64_t cid;
	/** time of zombification */
	uint64_t ztime;
};

struct mreq_find_zombies {
	struct mreq base;
	/** The lowest (cid, ztime) zombie to find */
	struct zombie_info lower_bound;
	/** Size of result buffer */
	int max_res;
	/** (out param) number of results found */
	int num_res;
	/** (out param) an array of size max_res where we'll store zombie
	 * information. */
	struct zombie_info *zinfos;
};

struct mreq_destroy_zombie {
	struct mreq base;
	/** zombie to destroy */
	struct zombie_info zinfo;
};

struct mreq_rename {
	struct mreq base;
	/** destination path */
	const char *dst_path;
};

struct mreq_node_search {
	struct mreq base;
	/** A node id which we must not recurse into.
	 * If this is RF_INVAL_NID, nothing is forbidden. */
	uint64_t forbidden;
	/** (out param)  The number of path components we failed to resolve.
	 * If we successfully resolved the path, this will be 0; if the last
	 * path component didn't exist, this will be 1; etc.
	 * This is only set if ret == 0 or ret == -ENOENT. */
	int npc_rem;
};

/** Initialize the metadata store.
 *
 * @param mgr		The fast log manager to use for fast logs
 * @param conf		The metadata store configuration
 *
 * @return		A pointer to the metadata store 0 on success; an error
 *			pointer otherwise.
 */
extern struct mstor* mstor_init(struct fast_log_mgr *mgr,
			const struct mstorc *conf, struct udata *udata);

/** Perform a blocking mstor operation
 *
 * @param mstor		The metadata store
 * @param mreq		The request
 *
 * @return		0 on success; error code otherwise
 */
extern int mstor_do_operation(struct mstor *mstor, struct mreq *mreq);

/** Shut down the metdata store
 *
 * @param mstor		The metadata store
 */
extern void mstor_shutdown(struct mstor *mstor);

/** Translate an mstor operation type to a string
 *
 * @param op		The mstor operation type
 *
 * @return		A statically allocated string representing the operation
 *			type
 */
extern const char *mstor_op_ty_to_str(enum mstor_op_ty op);

/** Dump the contents of the mstor out to a file
 *
 * @param mstor		The mstor
 * @param out		The file
 *
 * @return		0 on success; error code otherwise
 */
extern int mstor_dump(struct mstor *mstor, FILE *out);

#endif
