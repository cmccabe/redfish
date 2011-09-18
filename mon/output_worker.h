/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_OUTPUT_WORKER_DOT_H
#define ONEFISH_OUTPUT_WORKER_DOT_H

#include "mon/worker.h"

#include <unistd.h> /* for uint32_t */

#define LBUF_LEN_DIGITS 8

/** Mode to run the output worker in */
enum output_worker_sink_t {
	MON_OUTPUT_SINK_NONE,
	MON_OUTPUT_SINK_STDOUT,
	MON_OUTPUT_SINK_FISHTOP,
	MON_OUTPUT_SINK_NUM,
};

enum output_worker_json_msg_t {
	MON_OUTPUT_MSG_MON_CLUSTER = 1,
	MON_OUTPUT_MSG_END = 2,
};

/** The output worker */
extern struct worker *g_output_worker;

/** A message containing JSON for the output worker to output */
enum {
	WORKER_MSG_OUTPUT_JSON = 1,
};
struct output_worker_msg
{
	struct worker_msg msg;
	enum output_worker_json_msg_t json_ty;
	struct json_object* jo;
};

/** Initialize the output worker.
 *
 * @param argv0		argv[0]
 * @param mode		Output mode to use
 *
 * @return		0 on success; error code otherwise
 */
int output_worker_init(const char *argv0, enum output_worker_sink_t sink);

/** Stop and join the output worker
 *
 * @return		0 on success; error code otherwise
 */
int output_worker_shutdown(void);

/** Send a message to the output worker.
 *
 * @worker		the worker
 * @omsg		The message to send to the worker. The message will be
 *			freed, including the enclosed JSON, if the function
 *			returns nonzero.
 *
 * @return		0 on success; error code otherwise
 */
int output_worker_sendmsg_or_free(struct worker *worker,
				  struct output_worker_msg *omsg);

#endif
