/*
 * Copyright 2011-2012 the RedFish authors
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

#ifndef REDFISH_UTIL_FAST_LOG_TYPES_DOT_H
#define REDFISH_UTIL_FAST_LOG_TYPES_DOT_H

#include "util/bitfield.h"
#include "util/compiler.h"

#include <stdint.h>

enum fast_log_ty {
	FAST_LOG_MSGR_DEBUG = 1,
	FAST_LOG_MSGR_INFO,
	FAST_LOG_MSGR_ERROR,
	FAST_LOG_NUM_TYPES,
};

/** Convert a semi-colon delimited list of tokens to a bitfield representing
 * which fast log types to store
 *
 * @param str		The semicolon-delimited string
 * @param bits		(out-param) the bitfield
 * @param err		(out-param) error buffer
 * 			To complain about unused entries.
 * @param err_len	length of error buffer
 */
extern void str_to_fast_log_bitfield(const char *str,
		BITFIELD_DECL(bits, FAST_LOG_TYPE_MAX),
		char *err, size_t err_len);

/** Convert a single token to a bitfield representing
 * which fast logs types to store
 *
 * @param str		The string
 * @param bits		(out-param) the bitfield
 *
 * @return		0 if the string was recognized; -ENOENT otherwise
 */
extern int token_to_fast_log_bitfield(const char *str,
		BITFIELD_DECL(bits, FAST_LOG_TYPE_MAX));

extern const fast_log_dumper_fn_t g_fast_log_dumpers[];

#endif
