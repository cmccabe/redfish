/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_MON_INFO_DOT_H
#define ONEFISH_MON_INFO_DOT_H

#define JORM_CUR_FILE "mon/mon_info.jorm"
#include "jorm/jorm_generate_include.h"
#undef JORM_CUR_FILE

#include <pthread.h>

extern pthread_mutex_t g_mon_info_lock;
extern struct mon_info g_mon_info;

#endif
