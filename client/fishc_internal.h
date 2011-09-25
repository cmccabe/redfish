/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * * This is licensed under the Apache License, Version 2.0.
 */

#ifndef ONEFISH_CLIENT_FISHC_IMPL_DOT_H
#define ONEFISH_CLIENT_FISHC_IMPL_DOT_H

/** Onefish replication count
 * TODO: make this adjustable
 */
#define ONEFISH_FIXED_REPL 3

/** Onefish fixed 64 MB local buffer size
 * TODO: make this adjustable
 */
#define ONEFISH_FIXED_LBUF_SZ 67108864

/** Onefish fixed 64 MB chunk size
 * TODO: make this adjustable
 */
#define ONEFISH_FIXED_BLOCK_SZ 67108864

/** Default mode for files */
#define ONEFISH_DEFAULT_FILE_MODE 0644

/** Default mode for files */
#define ONEFISH_DEFAULT_DIR_MODE 0755

/** Maximum length of a onefish user name */
#define ONEFISH_USERNAME_MAX 255

/** Maximum length of a onefish group name */
#define ONEFISH_GROUPNAME_MAX 255

#endif
