/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MDS_MSTOR_DOT_H
#define REDFISH_MDS_MSTOR_DOT_H

#include <stdio.h> /* for FILE */
#include <stdint.h> /* uint32_t, etc. */

#define MNODE_IS_DIR 0x8000

struct fast_log_mgr;
struct mstor;

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
	MSTOR_OP_UNLINK,
	MSTOR_OP_UNLINK_TREE,
	MSTOR_OP_RENAME,
};

struct mreq {
	/** (caller sets) Operation type */
	enum mstor_op_ty op;
	/** (caller sets) Full path of request */
	const char *full_path;
	/** (caller sets) User performing request */
	const char *user;
	/** (caller sets) Group performing request */
	const char *group;
	/** (internal) Flags. */
	int flags;
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
