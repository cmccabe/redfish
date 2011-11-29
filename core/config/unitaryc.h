/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_CORE_CONFIG_UNITARYC_DOT_H
#define REDFISH_CORE_CONFIG_UNITARYC_DOT_H

#include "core/config/unitaryc.h"

#define JORM_CUR_FILE "core/config/unitaryc.jorm"
#include "jorm/jorm_generate_include.h"
#undef JORM_CUR_FILE

#if 0 /* Give the dependency scanner a clue */
#include "core/config/unitaryc.jorm"
#endif

#endif
