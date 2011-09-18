/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_CORE_LOG_CONFIG_DOT_H
#define ONEFISH_CORE_LOG_CONFIG_DOT_H

#include "core/daemon_type.h"

#define JORM_CUR_FILE "core/log_config.jorm"
#include "jorm/jorm_generate_include.h"
#undef JORM_CUR_FILE

#include <unistd.h> /* for size_t */

struct json_object;

/** Initialize the log_config structure
 *
 * @param jo		Configuration JSON
 * @param err		output buffer for errors
 * @param err_len	length of error buffer
 * @param dty		type of daemon we are
 *
 * @return		the log configuration. If an error occurs, NULL will be
 *			returned and the err buffer will be filled.
 */
struct log_config *create_log_config(struct json_object *jo,
		char *err, size_t err_len, enum fish_daemon_ty dty);

/** Free a log_config structure
 *
 * @param lconf		Pointer to a log_config structure
 *
 * @return		0 on success; error code otherwise
 */
void free_log_config(struct log_config *lconf);

#endif
