/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */
#ifndef REDFISH_UTIL_TEMPFILE_H
#define REDFISH_UTIL_TEMPFILE_H

/** Create a temporary directory
 *
 * @param tempdir	(out param) name buffer
 * @param name_max	length of tempdir buffer
 * @param mode		mode to use in mkdir 
 *
 * @return		0 on success; error code otherwise
 */
int get_tempdir(char *tempdir, int name_max, int mode);

/** Register a temporary directory to be deleted at the end of the program
 *
 * @param tempdir	The tempdir to register
 *
 * @return		0 on success; error code otherwise
 */
int register_tempdir_for_cleanup(const char *tempdir);

/** Unregister a temporary directory to be deleted at the end of the program
 *
 * @param tempdir	The tempdir to unregister
 */
void unregister_tempdir_for_cleanup(const char *tempdir);

/** Remove a tempdir
 *
 * @param tempdir	The tempdir to remove
 */
void remove_tempdir(const char *tempdir);

#endif
