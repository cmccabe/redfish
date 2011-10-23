/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MSG_MDS_DOT_H
#define REDFISH_MSG_MDS_DOT_H

#include "msg/generic.h"
#include "util/compiler.h"

#include <stdint.h>

#define DEFAULT_MDS_TO_MDS_PORT 9090
#define DEFAULT_OSD_TO_MDS_PORT 9091
#define DEFAULT_CLIENT_TO_MDS_PORT 9092

/* Network messages that can be sent to the metadata server */

enum {
	/** Client request to create a new file */
	MMM_CREATE_RFILE_REQ = 3000,
	/** Client request to open a new file */
	MMM_OPEN_RFILE_REQ,
	/** Chunk map sent from object storage daemon */
	MMM_CHUNK_REPORT,
	/** OSD heartbeat message */
	MMM_OSD_HEARTBEAT,
	/** Lookup some chunks for an open read-only file. */
	MMM_LOOKUP_CHUNKS_REQ,
	/** Get a new chunk for an open write-only file */
	MMM_ALLOC_CHUNK_REQ,
	/** Make a directory and all ancestors */
	MMM_MKDIRS_REQ,
	/** List all files in a directory */
	MMM_LIST_DIRECTORY_REQ,
	/** Give stat information regarding a path */
	MMM_STAT_REQ,
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
	/** Client close remote file */
	MMM_CLOSE_RFILE_REQ,
};

/* Create file */
PACKED_ALIGNED(8,
struct mmm_create_rfile_req {
	uint32_t block_sz;
	uint16_t mode;
	uint16_t repl;
	struct mmm_path path;
});
PACKED_ALIGNED(8,
struct mmm_open_rfile_req {
	uint16_t path_len;
	struct mmm_path path;
});
PACKED_ALIGNED(8,
struct mmm_chunk_report {
	/** Number of chunks, or a negative error value */
	int32_t num_chunks;
	uint64_t chunk_id[0];
});
PACKED_ALIGNED(8,
struct mmm_lookup_chunks_req {
	int64_t start;
	int64_t len;
	int32_t rfile;
});
PACKED_ALIGNED(8,
struct mmm_alloc_chunk_req {
	int32_t rfile;
	int32_t len;
	int64_t start;
});
PACKED_ALIGNED(8,
struct mmm_mkdirs_req {
	uint16_t mode;
	struct mmm_path path;
});
PACKED_ALIGNED(8,
struct mmm_list_directory_req {
	struct mmm_path path;
});
PACKED_ALIGNED(8,
struct mmm_stat_req {
	struct mmm_path path;
});
PACKED_ALIGNED(8,
struct mmm_chmod_req {
	uint16_t mode;
	struct mmm_path path;
});
PACKED_ALIGNED(8,
struct mmm_chown_req {
	uint16_t user;
	uint16_t group;
	struct mmm_path path;
});
PACKED_ALIGNED(8,
struct mmm_utimes_req {
	int64_t atime;
	int64_t mtime;
	struct mmm_path path;
});
PACKED_ALIGNED(8,
struct mmm_unlink_req {
	struct mmm_path path;
});
PACKED_ALIGNED(8,
struct mmm_rename_req {
	uint16_t msg_len;
	/* NULL-terminated source path, followed by NULL-terminated dest
	 * path */
	char data[0];
});
PACKED_ALIGNED(8,
struct mmm_close_rfile_req {
	int32_t rfile;
});

#endif
