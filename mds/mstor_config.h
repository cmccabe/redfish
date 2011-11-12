/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MDS_MSTOR_CONFIG_DOT_H
#define REDFISH_MDS_MSTOR_CONFIG_DOT_H

#include "mds/mstor_config.h"

#define JORM_CUR_FILE "mds/mstor_config.jorm"
#include "jorm/jorm_generate_include.h"
#undef JORM_CUR_FILE

#if 0 /* Give the dependency scanner a clue */
#include "mds/mstor_config.jorm"
#endif

#endif
