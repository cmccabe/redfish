/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "msg/fast_log.h"
#include "util/bitfield.h"
#include "util/fast_log.h"
#include "util/fast_log_types.h"
#include "util/macro.h"
#include "util/string.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BUILD_BUG_ON(FAST_LOG_TY_MAX > FAST_LOG_TYPE_MAX);

void str_to_fast_log_bitfield(const char *str,
		BITFIELD_DECL(bits, FAST_LOG_TYPE_MAX),
		char *err, size_t err_len)
{
	char *buf, *tok, *state = NULL;
	
	buf = strdup(str);
	if (!buf) {
		snprintf(err, err_len, "str_to_fast_log_bitfield: OOM");
		return;
	}
	for (tok = strtok_r(buf, ";", &state); tok;
			(tok = strtok_r(NULL, ";", &state))) {
		if (token_to_fast_log_bitfield(tok, bits)) {
			snappend(err, err_len, "No log type corresponds "
				"to \"%s\"\n", tok);
		}
	}
	free(buf);
}

int token_to_fast_log_bitfield(const char *str, BITFIELD_DECL(bits, FAST_LOG_TYPE_MAX))
{
	if (strcmp(str, "MSGR_DEBUG") == 0) {
		BITFIELD_SET(bits, FAST_LOG_TY_MSGR_DEBUG);
		return 0;
	}
	else if (strcmp(str, "MSGR_INFO") == 0) {
		BITFIELD_SET(bits, FAST_LOG_TY_MSGR_INFO);
		return 0;
	}
	else if (strcmp(str, "MSGR_WARN") == 0) {
		BITFIELD_SET(bits, FAST_LOG_TY_MSGR_WARN);
		return 0;
	}
	/* composites */
	else if (strcmp(str, "MSGR") == 0) {
		BITFIELD_SET(bits, FAST_LOG_TY_MSGR_DEBUG);
		BITFIELD_SET(bits, FAST_LOG_TY_MSGR_WARN);
		BITFIELD_SET(bits, FAST_LOG_TY_MSGR_INFO);
		return 0;
	}
	return -ENOENT;
}

#define STUB_FN(x) \
	WEAK_SYMBOL(void x(struct fast_log_entry *fe, char *buf)); \
	void x(POSSIBLY_UNUSED(struct fast_log_entry *fe), \
		      POSSIBLY_UNUSED(char *buf)) \
	{ \
	}

STUB_FN(fast_log_msgr_debug_dump);
STUB_FN(fast_log_msgr_info_dump);
STUB_FN(fast_log_msgr_warn_dump);

const fast_log_dumper_fn_t g_fast_log_dumpers[] = {
	[FAST_LOG_TY_MSGR_DEBUG] = fast_log_msgr_debug_dump,
	[FAST_LOG_TY_MSGR_INFO] = fast_log_msgr_info_dump,
	[FAST_LOG_TY_MSGR_WARN] = fast_log_msgr_warn_dump,
};
