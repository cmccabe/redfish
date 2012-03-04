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

#ifndef REDFISH_MSG_MDS_DOT_H
#define REDFISH_MSG_MDS_DOT_H

#include "mds/limits.h" /* for RF_MAX_MDS */
#include "msg/generic.h"
#include "util/bitfield.h" /* for BITFIELD_DECL */
#include "util/compiler.h"

#include <stdint.h>

#define DEFAULT_MDS_TO_MDS_PORT 9090
#define DEFAULT_OSD_TO_MDS_PORT 9091
#define DEFAULT_CLIENT_TO_MDS_PORT 9092

/* Network messages that can be sent to the metadata server */

enum {
	/** Get current MDS status */
	MMM_GET_MDS_STATUS = 3000,
	/** Current MDS status */
	MMM_MDS_STATUS,
	/** Client request to create a new file */
	MMM_CREATE_RFILE_REQ,
	/** Client request to open a new file */
	MMM_OPEN_RFILE_REQ,
	/** Response to a chunk examination message */
	MMM_CHUNK_EXAM_RESP,
	/** OSD heartbeat message */
	MMM_OSD_HEARTBEAT,
	/** Lookup some chunks for an open read-only file. */
	MMM_CHUNKFIND_REQ,
	/** Get a new chunk for an open write-only file */
	MMM_CHUNKALLOC_REQ,
	/** Make a directory and all ancestors */
	MMM_MKDIRS_REQ,
	/** List all files in a directory */
	MMM_LISTDIR_REQ,
	/** Give stat information regarding a path */
	MMM_PATH_STAT_REQ,
	/** Client Change permissions request */
	MMM_CHMOD_REQ,
	/** Client change ownership request */
	MMM_CHOWN_REQ,
	/** Client change atime/mtime request */
	MMM_UTIMES_REQ,
	/** Client unlink file request */
	MMM_UNLINK_REQ,
	/** Client unlink tree request */
	MMM_UNLINK_TREE_REQ,
	/** Client rename request */
	MMM_RENAME_REQ,
	/** Heartbeat request sent from peer */
	MMM_MDS_HEARTBEAT,
	/** Propose kicking metadata server(s) out of the cluster */
	MMM_PROPOSE_MDS_KICK,
	/** Response to a kick proposal */
	MMM_MDS_KICK_RESP,
	/** Commit a proposed MDS kick */
	MMM_COMMIT_MDS_KICK,
	/** Abort a proposed MDS kick */
	MMM_ABANDON_MDS_KICK,
};

PACKED(
struct mmm_get_mds_status {
	struct msg base;
});
PACKED(
struct mmm_mds_status {
	struct msg base;
	uint16_t mid;
	uint16_t pri_mid;
});
/* Create file */
PACKED(
struct mmm_create_rfile_req {
	struct msg base;
	uint32_t block_sz;
	uint16_t mode;
	uint16_t repl;
	uint64_t mtime;
	uint16_t path_len;
	char path[0];
});
PACKED(
struct mmm_open_rfile_req {
	struct msg base;
	uint64_t atime;
	uint16_t path_len;
	char path[0];
});
PACKED(
struct mmm_chunk_report {
	struct msg base;
	/** Number of chunks, or a negative error value */
	int32_t num_chunks;
	uint64_t chunk_id[0];
});
PACKED(
struct mmm_lookup_chunks_req {
	struct msg base;
	int64_t start;
	int64_t len;
	int32_t rfile;
});
PACKED(
struct mmm_alloc_chunk_req {
	struct msg base;
	uint64_t nid;
});
PACKED(
struct mmm_mkdirs_req {
	struct msg base;
	uint64_t mtime;
	uint16_t mode;
	uint16_t path_len;
	char path[0];
});
PACKED(
struct mmm_list_directory_req {
	struct msg base;
	uint16_t path_len;
	char path[0];
});
PACKED(
struct mmm_path_stat_req {
	struct msg base;
	char data[0];
	/* path */
});
PACKED(
struct mmm_chmod_req {
	struct msg base;
	uint16_t mode;
	char data[0];
	/* path */
});
PACKED(
struct mmm_chown_req {
	struct msg base;
	char data[0];
	/* path */
	/* user */
	/* group */
});
PACKED(
struct mmm_utimes_req {
	struct msg base;
	int64_t atime;
	int64_t mtime;
	uint16_t path_len;
	char path[0];
});
PACKED(
struct mmm_unlink_req {
	struct msg base;
	uint16_t path_len;
	char path[0];
});
PACKED(
struct mmm_rename_req {
	struct msg base;
	uint16_t msg_len;
	/* NULL-terminated source path, followed by NULL-terminated dest
	 * path */
	char data[0];
});
PACKED(
struct mmm_close_rfile_req {
	struct msg base;
	int32_t rfile;
});
PACKED(
struct mmm_mds_heartbeat {
	struct msg base;
	uint16_t mid;
});
PACKED(
struct mmm_propose_mds_kick {
	struct msg base;
	uint64_t cookie;
	BITFIELD_DECL(kicked_peers, RF_MAX_MDS);
	uint16_t mid;
});
PACKED(
struct mmm_mds_kick_resp {
	struct msg base;
	uint64_t cookie;
	uint16_t resp;
});
PACKED(
struct mmm_commit_mds_kick {
	struct msg base;
	uint64_t cookie;
});
PACKED(
struct mmm_abandon_mds_kick {
	struct msg base;
	uint64_t cookie;
});

#endif
