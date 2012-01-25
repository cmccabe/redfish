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
