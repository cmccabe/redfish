/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "mon/mon_info.h"

#define JORM_CUR_FILE "mon/mon_info.jorm"
#include "jorm/jorm_generate_body.h"
#undef JORM_CUR_FILE

pthread_mutex_t g_mon_info_lock = PTHREAD_MUTEX_INITIALIZER;
struct mon_info g_mon_info = { NULL, NULL};
