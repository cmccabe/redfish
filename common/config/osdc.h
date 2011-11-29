/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_COMMON_CONFIG_OSDC_DOT_H
#define REDFISH_COMMON_CONFIG_OSDC_DOT_H

#include "common/config/logc.h"

#define JORM_CUR_FILE "common/config/osdc.jorm"
#include "jorm/jorm_generate_include.h"
#undef JORM_CUR_FILE

#if 0 /* Give the dependency scanner a clue */
#include "common/config/osdc.jorm"
#endif

#endif
