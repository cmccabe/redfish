/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_MDS_NET_DOT_H
#define REDFISH_MDS_NET_DOT_H

/** Runs the main metadata server loop
 *
 * @return	0 on successful exit; error code otherwise
 */
int mds_main_loop(void);

#endif
