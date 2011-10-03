/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * * This is licensed under the Apache License, Version 2.0.
 */

#ifndef REDFISH_CLIENT_FISHC_IMPL_DOT_H
#define REDFISH_CLIENT_FISHC_IMPL_DOT_H

/** Default MDS port */
#define REDFISH_DEFAULT_MDS_PORT 9000

/** RedFish replication count
 * TODO: make this adjustable
 */
#define REDFISH_FIXED_REPL 3

/** RedFish fixed 64 MB local buffer size
 * TODO: make this adjustable
 */
#define REDFISH_FIXED_LBUF_SZ 67108864

/** RedFish fixed 64 MB chunk size
 * TODO: make this adjustable
 */
#define REDFISH_FIXED_BLOCK_SZ 67108864

/** Default mode for files */
#define REDFISH_DEFAULT_FILE_MODE 0644

/** Default mode for files */
#define REDFISH_DEFAULT_DIR_MODE 0755

/** Maximum length of a redfish user name */
#define REDFISH_USERNAME_MAX 255

/** Maximum length of a redfish group name */
#define REDFISH_GROUPNAME_MAX 255

#endif
