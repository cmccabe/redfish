/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */
#ifndef ONEFISH_UTIL_TEMPFILE_H
#define ONEFISH_UTIL_TEMPFILE_H

/** Create a temporary directory */
int get_tempdir(char *tempdir, int name_max, int mode);

/** Register a temporary directory to be deleted at the end of the program */
int register_tempdir_for_cleanup(const char *tempdir);

#endif
