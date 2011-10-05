/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_CORE_FAST_LOG_TYPES_DOT_H
#define REDFISH_CORE_FAST_LOG_TYPES_DOT_H

#include "util/compiler.h"

#include <stdint.h>

struct fast_log_buf;

extern void fast_log_new_conn(struct fast_log_buf *fb, uint32_t conn_ty,
	uint32_t fd, unsigned long s_addr);

extern void fast_log_close_conn(struct fast_log_buf *fb,
	uint32_t conn_ty, uint32_t fd, unsigned long s_addr, int32_t error);

extern const fast_log_dumper_fn_t g_fast_log_dumpers[];

#endif
