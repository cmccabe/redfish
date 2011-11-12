/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_OSD_OSD_CONFIG_DOT_H
#define REDFISH_OSD_OSD_CONFIG_DOT_H

#include "core/log_config.h"

#define JORM_CUR_FILE "osd/osd_config.jorm"
#include "jorm/jorm_generate_include.h"
#undef JORM_CUR_FILE

#if 0 /* Give the dependency scanner a clue */
#include "osd/osd_config.jorm"
#endif

#endif
