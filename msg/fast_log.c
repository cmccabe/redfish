/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "msg/fast_log.h"
#include "msg/generic.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/macro.h"

#include <arpa/inet.h>
#include <errno.h>
#include <ev.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fast_log_msgr_debug_dump(POSSIBLY_UNUSED(struct fast_log_entry *fe), POSSIBLY_UNUSED(char *buf))
{
}

void fast_log_msgr_info_dump(POSSIBLY_UNUSED(struct fast_log_entry *fe), POSSIBLY_UNUSED(char *buf))
{
}

void fast_log_msgr_warn_dump(POSSIBLY_UNUSED(struct fast_log_entry *fe), POSSIBLY_UNUSED(char *buf))
{
}
