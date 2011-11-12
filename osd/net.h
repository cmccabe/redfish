/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_OSD_NET_DOT_H
#define REDFISH_OSD_NET_DOT_H

struct osd_config;

/** Start the object storage daemon main loop
 *
 * @param conf		The daemon configuration
 *
 * @return		The return value of the program
 */
extern int osd_main_loop(struct osd_config *conf);

#endif
