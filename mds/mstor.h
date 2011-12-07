/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MDS_MSTOR_DOT_H
#define REDFISH_MDS_MSTOR_DOT_H

#include <stdint.h> /* uint32_t, etc. */

struct fast_log_mgr;
struct mstor;

enum mstor_op {
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
	MSTOR_OP_UNLINK,
	MSTOR_OP_UNLINK_TREE,
	MSTOR_OP_RENAME,
};

struct mreq {
	/** Operation type */
	enum mstor_op op;
	/** Full path of request */
	const char *full_path;
	/** User performing request */
	const char *user;
	/** Group performing request */
	const char *group;
	/** Operation type-specific data */
	char data[0];
};

struct mreq_creat {
	struct mreq base;
	/** Mode to create file with */
	uint16_t mode;
	/** mtime / atime to create file with */
	uint64_t mtime;
};

struct mreq_mkdirs {
	struct mreq base;
	/** Mode to create directory with */
	uint16_t mode;
	/** mtime / atime to create directory with */
	uint64_t mtime;
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
				const struct mstorc *conf);

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

#endif
