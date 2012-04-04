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

const MMM_PACKED_STAT_IS_DIR = 0x8000;

/** maximum length of a path name in Redfish */
const RF_PATH_MAX = 4096;

/** Maximum length of a user name */
const RF_USER_MAX = 64;

/** Maximum length of a group name */
const RF_GROUP_MAX = 64;

/** maximum number of OSDs that will be used store a single chunk */
const RF_MAX_OID = 7;

/** Maximum files per directory.  This can go away when we get the ability to
 * do a partial listdir() */
const RF_MAX_FILES_PER_LISTDIR = 100000;

/** Describes an endpoint */
struct endpoint {
	int ip;
	int port;
};

/** Describes the stat() information stored with a file. */
struct rf_stat {
	unsigned hyper mtime;
	unsigned hyper atime;
	unsigned hyper length;
	unsigned hyper block_sz;
	int mode_and_type;
	int man_repl;
	string user<RF_USER_MAX>;
	string group<RF_GROUP_MAX>;
};

/** maximum length of a path component in Redfish */
const RF_PCOMP_MAX = 256;

/** A path,stat combo returned by listdir */
struct rf_lentry {
	string pcomp<RF_PCOMP_MAX>;
	struct rf_stat stat;
};

enum fish_msg_ty {
	/* ============== Common ============== */
	/** Generic response */
	mmm_resp_ty = 1000,
	/** heartbeat message */
	mmm_heartbeat_ty,
	/** get current daemon status */
	mmm_status_req_ty,

	/* ============== client requests ============== */
	/** Client request to set a user's primary group */
	mmm_set_primary_user_group_ty,
	/** Client request to add a user to a group */
	mmm_add_user_to_group_ty,
	/** Client request to remove a user from a group */
	mmm_remove_user_from_group_ty,
	/** client request to create a new file */
	mmm_create_file_req_ty = 2000,
	/** client request to open a new file */
	mmm_open_file_req_ty,
	/** make a directory and all ancestors */
	mmm_mkdirs_req_ty,
	/** list all files in a directory */
	mmm_listdir_req_ty,
	/** give stat information regarding a path */
	mmm_path_stat_req_ty,
	/** give stat information regarding a nid */
	mmm_nid_stat_req_ty,
	/** client change permissions request */
	mmm_chmod_req_ty,
	/** client change ownership request */
	mmm_chown_req_ty,
	/** client change atime/mtime request */
	mmm_utimes_req_ty,
	/** client unlink request */
	mmm_unlink_req_ty,
	/** client rename request */
	mmm_rename_req_ty,

	/* ============== mds messages ============== */
	/** current mds status */
	mmm_mds_status_resp_ty = 3000,
	/** a new chunk id for the client.  the mds will send this in
	 * response to a 'create file' request_ty, and also if the client requests
	 * a new chunk id for a file. */
	mmm_chunkalloc_resp_ty,
	/** mds response to an stat request */
	mmm_stat_resp_ty,
	/** mds response to a 'list directory' request */
	mmm_listdir_resp_ty,
	/** osd response to a request for a chunk */
	mmm_fetch_chunk_resp_ty,

	/* ============== osd messages ============== */
	/** request to read from the osd */
	mmm_osd_read_req_ty = 4000,
	/** response to mmm_osd_read_req */
	mmm_osd_read_resp_ty,
	/** client request to write a chunk to the osd */
	mmm_osd_hflush_req_ty,
	/** mds request to get information about chunks */
	mmm_osd_chunkrep_req_ty,
	/** osd response to chunk report request */
	mmm_osd_chunkrep_resp_ty,
	/** mds request to unlink a chunk */
	mmm_osd_unlink_req_ty
};

/* ============== Common ============== */
struct mmm_resp {
	unsigned int error;
};

struct mmm_heartbeat {
	unsigned int ty;
	unsigned int id;
};

struct mmm_status_req {
	int flags;
};

/* ============== Client Requests ============== */
struct mmm_set_primary_user_group {
	string user<RF_USER_MAX>;
	string tgt_user<RF_USER_MAX>;
	string new_pri_group<RF_GROUP_MAX>;
};

struct mmm_add_user_to_group {
	string user<RF_USER_MAX>;
	string tgt_user<RF_USER_MAX>;
	string group<RF_GROUP_MAX>;
};

struct mmm_remove_user_from_group {
	string user<RF_USER_MAX>;
	string tgt_user<RF_USER_MAX>;
	string group<RF_GROUP_MAX>;
};

struct mmm_create_file_req {
	unsigned int block_sz;
	unsigned int mode;
	unsigned int repl;
	unsigned hyper mtime;
	string path<RF_PATH_MAX>;
	string user<RF_USER_MAX>;
};

struct mmm_open_file_req {
	uint64_t atime;
	uint16_t path_len;
	string path<RF_PATH_MAX>;
	string user<RF_USER_MAX>;
};

struct mmm_mkdirs_req {
	unsigned hyper ctime;
	int mode;
	string path<RF_PATH_MAX>;
	string user<RF_USER_MAX>;
};

struct mmm_listdir_req {
	string path<RF_PATH_MAX>;
	string user<RF_USER_MAX>;
};

struct mmm_path_stat_req {
	string path<RF_PATH_MAX>;
	string user<RF_USER_MAX>;
};

struct mmm_nid_stat_req {
	unsigned hyper nid;
	string user<RF_USER_MAX>;
};

struct mmm_chmod_req {
	int mode;
	string user<RF_USER_MAX>;
	string path<RF_PATH_MAX>;
};

struct mmm_chown_req {
	string path<RF_PATH_MAX>;
	string user<RF_USER_MAX>;
	string new_user<RF_USER_MAX>;
	string new_group<RF_GROUP_MAX>;
};

struct mmm_utimes_req {
	unsigned hyper new_atime;
	unsigned hyper new_mtime;
	string path<RF_PATH_MAX>;
	string user<RF_USER_MAX>;
};

enum mmm_unlink_op {
	/** Unlink a single file */
	MMM_UOP_UNLINK = 1,
	/** POSIX rmdir */
	MMM_UOP_RMDIR = 2,
	/** Remove recursively */
	MMM_UOP_RMRF = 4
};

struct mmm_unlink_req {
	enum mmm_unlink_op uop;
	string path<RF_PATH_MAX>;
	string user<RF_USER_MAX>;
};

struct mmm_rename_req {
	string src<RF_PATH_MAX>;
	string dst<RF_PATH_MAX>;
	string user<RF_USER_MAX>;
};

/* ============== MDS messages ============== */
struct mmm_mds_status_resp {
	int mid;
	int pri_mid;
};

struct mmm_chunkalloc_resp {
	unsigned hyper cid;
	struct endpoint ep<RF_MAX_OID>;
};

struct mmm_stat_resp {
	struct rf_stat stat;
};

struct mmm_listdir_resp {
	struct rf_lentry le<RF_MAX_FILES_PER_LISTDIR>;
};

const MMM_OSD_FETCH_CHUNK_LEN_MAX = 2147483648;

struct mmm_fetch_chunk_resp {
	unsigned hyper cid;
	/* next: data */
};

/* ============== OSD messages ============== */
struct mmm_osd_read_req {
	unsigned hyper cid;
	unsigned hyper start;
	int len;
};

struct mmm_osd_read_resp {
	int flags;
	/* next: data */
};

const MMM_OSD_HFLUSH_DATA_MAX = 2147483648;

struct mmm_osd_hflush_req {
	unsigned hyper cid;
	int flags;
	/* next: data */
};

const MMM_OSD_CHUNKREP_MAX_CHUNKS = 131072;

struct mmm_osd_chunkrep_req {
	unsigned hyper cid<MMM_OSD_CHUNKREP_MAX_CHUNKS>;
};

struct mmm_osd_chunkrep_resp {
	int flags;
};

struct mmm_osd_unlink_req {
	unsigned hyper cid;
};

struct mmm_create_file_resp {
	uint64_t nid;
};
struct mmm_open_rfile_resp {
	uint64_t nid;
	uint32_t chunk_addr;
	uint64_t chunk_id;
};
