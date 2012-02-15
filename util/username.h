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

#ifndef REDFISH_UTIL_USERNAME_DOT_H
#define REDFISH_UTIL_USERNAME_DOT_H

#include <unistd.h> /* for size_t */

/** Find the current username
 *
 * @param out		(out param) the username
 * @param out_len	length of the out buffer
 *
 * @return		0 on success; -ENAMETOOLONG if the buffer is too short,
 * 			-ENOSYS if the username could not be found.
 */
extern int get_current_username(char *out, size_t out_len);

/** Get the user ID associated with a user name
 *
 * @param username	The user name
 * @param uid		(out param) the user id
 *
 * @return		0 on success; error code otherwise
 */
extern int get_user_id(const char *username, uid_t *uid);

/** Get the user ID associated with a user name
 *
 * @param uid		the user id
 * @param username	(out param) The user name
 * @param username_len	length of the username buffer
 *
 * @return		0 on success; error code otherwise
 */
extern int get_user_name(uid_t uid, char *username, size_t username_len);

/** Get the user ID associated with a user name
 *
 * @param username	The group name
 * @param gid		(out param) the group id
 *
 * @return		0 on success; error code otherwise
 */
extern int get_group_id(const char *username, gid_t *gid);

/** Get the user ID associated with a user name
 *
 * @param gid		the group id
 * @param groupname	(out param) The group name
 * @param groupname_len	length of the group name buffer
 *
 * @return		0 on success; error code otherwise
 */
extern int get_group_name(uid_t uid, char *groupname, size_t groupname_len);

#endif
