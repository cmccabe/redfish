/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_UTIL_FAST_LOG_TYPES_DOT_H
#define REDFISH_UTIL_FAST_LOG_TYPES_DOT_H

#include "util/bitfield.h"
#include "util/compiler.h"

#include <stdint.h>

enum fast_log_ty {
	FAST_LOG_TY_MSGR_DEBUG = 0,
	FAST_LOG_TY_MSGR_INFO,
	FAST_LOG_TY_MSGR_WARN,
	FAST_LOG_TY_MAX,
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
