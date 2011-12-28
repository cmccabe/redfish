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

/** maximum number of replicas for any delegation */
#define RF_MAX_REPLICAS 7

/** maximum length of a path component in RedFish */
#define RF_PCOMP_MAX 256

/** maximum number of MDSes in a cluster */
#define MAX_MDS 64

/** Maximum length of a user name */
#define RF_USER_MAX 64

/** Maximum length of a group name */
#define RF_GROUP_MAX 64

/** The superuser */
#define RF_SUPERUSER "superuser"

/** Invalid (u)time */
#define RF_INVAL_TIME 0xffffffffffffffffULL

/** Invalid user ID */
#define RF_INVAL_UID 0xffffffffUL

/** Invalid group ID */
#define RF_INVAL_GID 0xffffffffUL

#endif
