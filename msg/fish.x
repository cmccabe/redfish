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

enum fish_msg_ty {
        /* ============== Common ============== */
        /** Generic response */
	MMM_RESP = 1000,
	/** Heartbeat message */
	MMM_HEARTBEAT,
	/** Get current daemon status */
	MMM_STATUS_REQ,

        /* ============== Client Requests ============== */
	/** Client request to create a new file */
	MMM_CREATE_FILE_REQ = 2000,
	/** Client request to open a new file */
	MMM_OPEN_FILE_REQ,
	/** Make a directory and all ancestors */
	MMM_MKDIRS_REQ,
	/** List all files in a directory */
	MMM_LISTDIR_REQ,
	/** Give stat information regarding a path */
	MMM_PATH_STAT_REQ,
	/** Give stat information regarding a nid */
	MMM_NID_STAT_REQ,
	/** Client Change permissions request */
	MMM_CHMOD_REQ,
	/** Client change ownership request */
	MMM_CHOWN_REQ,
	/** Client change atime/mtime request */
	MMM_UTIMES_REQ,
	/** Client unlink request */
	MMM_UNLINK_REQ,
	/** Client rename request */
	MMM_RENAME_REQ,

        /* ============== MDS messages ============== */
	/** Current MDS status */
	MMM_MDS_STATUS_RESP = 3000,
	/** A new chunk ID for the client.  The MDS will send this in
	 * response to a 'create file' request, and also if the client requests
	 * a new chunk ID for a file. */
	MMM_CHUNKALLOC_RESP,
	/** MDS response to an stat request */
	MMM_STAT_RESP,
	/** MDS response to a 'list directory' request */
	MMM_LISTDIR_RESP,
	/** OSD response to a request for a chunk */
	MMM_FETCH_CHUNK_RESP,

        /* ============== OSD messages ============== */
	/** Request to read from the OSD */
	MMM_OSD_READ_REQ = 4000,
	/** Response to MMM_OSD_READ_REQ */
	MMM_OSD_READ_RESP,
	/** Client request to write a chunk to the OSD */
	MMM_OSD_HFLUSH_REQ,
	/** MDS request to get information about chunks */
	MMM_OSD_CHUNKREP_REQ,
	/** OSD response to chunk report request */
	MMM_OSD_CHUNKREP_RESP,
	/** MDS request to unlink a chunk */
	MMM_OSD_UNLINK_REQ
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
	opaque data<0>;
};

/* ============== Client Requests ============== */
struct mmm_create_file_req {
	unsigned int block_sz;
	unsigned int mode;
	unsigned int repl;
	unsigned hyper mtime;
	string path<RF_PATH_MAX>;
};

struct mmm_open_file_req {
	uint64_t atime;
	uint16_t path_len;
	string path<RF_PATH_MAX>;
};

struct mmm_mkdirs_req {
	unsigned hyper mtime;
	int mode;
        string path<RF_PATH_MAX>;
};

struct mmm_listdir_req {
        string path<RF_PATH_MAX>;
};

struct mmm_path_stat_req {
        string path<RF_PATH_MAX>;
};

struct mmm_nid_stat_req {
	unsigned hyper nid;
};

struct mmm_chmod_req {
	int mode;
        string path<RF_PATH_MAX>;
};

struct mmm_chown_req {
        string path<RF_PATH_MAX>;
        string user<RF_USER_MAX>;
        string group<RF_GROUP_MAX>;
};

struct mmm_utimes_req {
	unsigned hyper atime;
	unsigned hyper mtime;
        string path<RF_PATH_MAX>;
};

/** Flag for MMM_UNLINK that means remove recursively */
const MMM_UNLINK_RECURSIVE = 1;
/** Flag for MMM_UNLINK that means implement POSIX rmdir semantics */
const MMM_UNLINK_POSIX_RMDIR = 2;

struct mmm_unlink_req {
	int flags;
        string path<RF_PATH_MAX>;
};

struct mmm_rename_req {
        string src<RF_PATH_MAX>;
        string dst<RF_PATH_MAX>;
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
	struct rf_stat stat<RF_MAX_FILES_PER_LISTDIR>;
};

const MMM_OSD_FETCH_CHUNK_LEN_MAX = 2147483648;

struct mmm_fetch_chunk_resp {
	unsigned hyper cid;
	opaque data<MMM_OSD_FETCH_CHUNK_LEN_MAX>;
};

/* ============== OSD messages ============== */
struct mmm_osd_read_req {
	unsigned hyper cid;
	unsigned hyper start;
	int len;
};

const MMM_OSD_HFLUSH_DATA_MAX = 2147483648;

struct mmm_osd_hflush_req {
	unsigned hyper cid;
	int flags;
	opaque data<MMM_OSD_HFLUSH_DATA_MAX>;
};

const MMM_OSD_CHUNKREP_MAX_CHUNKS = 131072;

struct mmm_osd_chunkrep_req {
	unsigned hyper cid<MMM_OSD_CHUNKREP_MAX_CHUNKS>;
};

struct mmm_chunkrep_resp_chunk {
	unsigned hyper cid;
	int flags;
};

struct mmm_osd_chunkrep_resp {
	struct mmm_chunkrep_resp_chunk ch<MMM_OSD_CHUNKREP_MAX_CHUNKS>;
};

struct mmm_osd_unlink_req {
	unsigned hyper cid;
};

struct mmm_create_file_resp {
	uint64_t nid;
	uint8_t num_ep;
	char data[0];
};
struct mmm_open_rfile_resp {
	uint64_t nid;
	uint32_t chunk_addr;
	uint64_t chunk_id;
};


