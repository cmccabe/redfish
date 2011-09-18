/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_CORE_DAEMON_TYPE_H
#define ONEFISH_CORE_DAEMON_TYPE_H

enum fish_daemon_ty
{
	ONEFISH_DAEMON_TYPE_OSD = 0,
	ONEFISH_DAEMON_TYPE_MDS = 1,
	ONEFISH_DAEMON_TYPE_MON = 2,
	ONEFISH_DAEMON_TYPE_NUM = 3,
};

#endif
