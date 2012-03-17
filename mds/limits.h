/*
 * Copyright 2011-2012 the Redfish authors
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

/** maximum length of a path name in Redfish */
#define RF_PATH_MAX 4096

/** maximum number of OSDs that will be used store a single chunk */
#define RF_MAX_OID 7

/** maximum number of replicas for any delegation */
#define RF_MAX_REPLICAS 7

/** maximum length of a path component in Redfish */
#define RF_PCOMP_MAX 256

/** maximum number of MDSes in a cluster */
#define RF_MAX_MDS 254

/** invalid MDS ID */
#define RF_INVAL_MID 255

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
#define RF_NOBODY_GID 1

/** Invalid (u)time */
#define RF_INVAL_TIME 0xffffffffffffffffULL

/** Invalid user ID */
#define RF_INVAL_UID 0xffffffffUL

/** Invalid group ID */
#define RF_INVAL_GID 0xffffffffUL

/** Invalid node ID */
#define RF_INVAL_NID 0xffffffffffffffffULL

/** Invalid chunk ID */
#define RF_INVAL_CID 0xffffffffffffffffULL

/** Invalid filesystem ID */
#define RF_INVAL_FSID 0

/** The root delegation ID */
#define RF_ROOT_DGID 0ULL

/** Invalid delegation ID */
#define RF_INVAL_DGID 0xffffffffffffffffULL

#define MSTOR_ROOT_NID_INIT_MODE 0755

#endif
