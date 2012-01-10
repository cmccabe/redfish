/*
 * Copyright 2011-2012 the RedFish authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
#define RF_SUPERUSER_NAME "superuser"
#define RF_SUPERUSER_UID 0
#define RF_SUPERUSER_GID 0

/** nobody */
#define RF_NOBODY_NAME "nobody"
#define RF_NOBODY_UID 1

/** everybody */
#define RF_EVERYONE_NAME "everyone"
#define RF_EVERYONE_GID 1

/** Invalid (u)time */
#define RF_INVAL_TIME 0xffffffffffffffffULL

/** Invalid user ID */
#define RF_INVAL_UID 0xffffffffUL

/** Invalid group ID */
#define RF_INVAL_GID 0xffffffffUL

#endif
