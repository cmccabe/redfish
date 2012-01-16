/*
 * Copyright 2011-2012 the RedFish authors
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

#include "mds/limits.h"

#define MNODE_IS_DIR 0x8000

struct fast_log_mgr;
struct mstor;
struct udata;

enum mstor_op_ty {
	MSTOR_OP_CREAT = 1,
	MSTOR_OP_OPEN,
	MSTOR_OP_CHUNKFIND,
	MSTOR_OP_CHUNKALLOC,
	MSTOR_OP_MKDIRS,
	MSTOR_OP_LISTDIR,
	MSTOR_OP_STAT,
	MSTOR_OP_CHMOD,
	MSTOR_OP_CHOWN,
	MSTOR_OP_UTIMES,
	MSTOR_OP_RMDIR,
	MSTOR_OP_UNLINK,
	MSTOR_OP_FIND_ZOMBIES,
	MSTOR_OP_DESTROY_ZOMBIE,
	MSTOR_OP_RENAME,
	/** for internal use only */
	MSTOR_OP_NODE_SEARCH,
};

struct mreq {
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
	uint64_t start;
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
	/** (out param) retrieved chunk information */
	struct chunk_info cinfos[0];
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
	/** Pointer to buffer to use to return the results.
	 * The result will be returned as a packed stat entry. */
	char *out;
	/** Length of the out buffer. */
	size_t out_len;
};

struct mreq_listdir {
	struct mreq base;
	/** Pointer to buffer to use to return the results.
	 * The results will be returned as a series of packed stat entries. */
	char *out;
	/** Length of the out buffer. */
	size_t out_len;
	/** Length used in the out buffer. */
	size_t used_len;
};

struct mreq_chown {
	struct mreq base;
	/** New owner, or NULL for no change */
	const char *new_user;
	/** New group, or NULL for no change */
	const char *new_group;
};

struct mreq_chmod {
	struct mreq base;
	/** New mode (type bits are not included) */
	uint16_t mode;
};

struct mreq_utimes {
	struct mreq base;
	/** New modification time, or RF_INVAL_TIME if the modification time
	 * should not change.  */
	uint64_t mtime;
	/** New access time, or RF_INVAL_TIME if the access time should not
	 * change.  */
	uint64_t atime;
};

struct mreq_rmdir {
	struct mreq base;
	/** Time of rmdir */
	uint64_t ztime;
	/** If 0 -- POSIX rmdir behavior: succeed if the directory is empty,
	 * fail otherwise.
	 * If 1 -- try to delete all the files in this directory.
	 * Fail if we encounter a directory entry or a file we cannot delete.
	 *
	 * This is a simple optimization to make it easier for clients to
	 * implement Hadoop-style recursive rm.
	 */
	int rmr;
};

struct mreq_unlink {
	struct mreq base;
	/** Time of unlink */
	uint64_t ztime;
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
	/** (out param) results */
	struct zombie_info zinfo[0];
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
	/** (out param)  If we successfully resolve the path, this is the path
	 * component of the child. */
	char pcomp[RF_PCOMP_MAX];
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
