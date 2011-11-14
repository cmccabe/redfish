/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MDS_LIMITS_DOT_H
#define REDFISH_MDS_LIMITS_DOT_H

/** maximum length of a path name in RedFish */
#define RF_PATH_MAX 4096

/** maximum length of a path component in RedFish */
#define RF_PATH_COMPONENT_MAX 256

/** maximum number of MDSes in a cluster */
#define MAX_MDS 64

/** Maximum length of a user name */
#define USER_MAX 64

/** Maximum length of a group name */
#define GROUP_MAX 64

#endif
