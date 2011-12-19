/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MDS_USER_DOT_H
#define REDFISH_MDS_USER_DOT_H

/** Create a user data lookup cache
 *
 * @return		The user data, or an error pointer on failure
 */
extern struct udata *udata_create(void);

/** Free a user data lookup cache
 *
 * @param udata		The user data
 */
extern void udata_free(struct udata *udata);

/** Determine if a user is in a group
 *
 * @param udata		The user data
 * @param user		The user
 *
 * @return		0 if the user is not in the group; 1 otherwise
 */
extern int user_in_group(const struct udata *udata,
		const char *user, const char *group);

/** Find the primary group of a user
 *
 * @param udata		The user data
 * @param user		The user
 *
 * @return		The user's primary group
 *			NULL if the user is unknown
 */
extern const char *user_primary_group(const struct udata *udata,
		const char *user);

#endif
