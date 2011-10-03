/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_OSD_NET_DOT_H
#define REDFISH_OSD_NET_DOT_H

struct daemon;

/** Start the object storage daemon main loop
 *
 * @param d		The daemon configuration
 *
 * @return		The return value of the program
 */
extern int osd_main_loop(struct daemon *d);

#endif
