/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_CORE_DAEMON_TYPE_H
#define REDFISH_CORE_DAEMON_TYPE_H

enum fish_daemon_ty
{
	REDFISH_DAEMON_TYPE_OSD = 0,
	REDFISH_DAEMON_TYPE_MDS = 1,
	REDFISH_DAEMON_TYPE_MON = 2,
	REDFISH_DAEMON_TYPE_NUM = 3,
};

#endif
