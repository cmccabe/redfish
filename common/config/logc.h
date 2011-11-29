/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_COMMON_CONFIG_LOGC_DOT_H
#define REDFISH_COMMON_CONFIG_LOGC_DOT_H

#define JORM_CUR_FILE "common/config/logc.jorm"
#include "jorm/jorm_generate_include.h"
#undef JORM_CUR_FILE

#if 0 /* Give the dependency scanner a clue */
#include "common/config/logc.jorm"
#endif

#include <unistd.h> /* for size_t */

struct json_object;

/** Harmonize the log_config structure.
 * Mostly, this means filling in defaults based on base_dir.
 *
 * @param lc		The log_config
 * @param err		output buffer for errors
 * @param err_len	length of error buffer
 * @param want_mkdir	True if we want to make base_dir if it doesn't exist
 */
void harmonize_logc(struct logc *lc,
		char *err, size_t err_len, int want_mkdir);

#endif
