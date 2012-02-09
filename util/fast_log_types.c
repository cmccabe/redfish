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

BUILD_BUG_ON(FAST_LOG_NUM_TYPES > FAST_LOG_TYPE_MAX);

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
		BITFIELD_SET(bits, FAST_LOG_MSGR_DEBUG);
		return 0;
	}
	else if (strcmp(str, "MSGR_INFO") == 0) {
		BITFIELD_SET(bits, FAST_LOG_MSGR_INFO);
		return 0;
	}
	else if (strcmp(str, "MSGR_ERROR") == 0) {
		BITFIELD_SET(bits, FAST_LOG_MSGR_ERROR);
		return 0;
	}
	/* composites */
	else if (strcmp(str, "MSGR") == 0) {
		BITFIELD_SET(bits, FAST_LOG_MSGR_DEBUG);
		BITFIELD_SET(bits, FAST_LOG_MSGR_INFO);
		BITFIELD_SET(bits, FAST_LOG_MSGR_ERROR);
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
STUB_FN(fast_log_msgr_error_dump);
STUB_FN(fast_log_bsend_debug_dump);
STUB_FN(fast_log_bsend_error_dump);

const fast_log_dumper_fn_t g_fast_log_dumpers[] = {
	[FAST_LOG_MSGR_DEBUG] = fast_log_msgr_debug_dump,
	[FAST_LOG_MSGR_INFO] = fast_log_msgr_info_dump,
	[FAST_LOG_MSGR_ERROR] = fast_log_msgr_error_dump,
	[FAST_LOG_BSEND_DEBUG] = fast_log_bsend_debug_dump,
	[FAST_LOG_BSEND_ERROR] = fast_log_bsend_error_dump,
};
