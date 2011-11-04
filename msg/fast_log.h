/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MSG_FAST_LOG_DOT_H
#define REDFISH_MSG_FAST_LOG_DOT_H

#include "util/fast_log.h"

#include <stdint.h>

extern void fast_log_msgr_debug_dump(struct fast_log_entry *fe, char *buf);

extern void fast_log_msgr_info_dump(struct fast_log_entry *fe, char *buf);

extern void fast_log_msgr_warn_dump(struct fast_log_entry *fe, char *buf);

#endif
