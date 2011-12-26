/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * * This is licensed under the Apache License, Version 2.0.
 */

#include "client/fishc.h"
#include "util/macro.h"

#include <stdint.h>

#define RF_VERSION_MAJOR 1
#define RF_VERSION_MINOR 0
#define RF_VERSION_PATCHLEVEL 0

#define RF_VERSION_STR \
	TO_STR2(RF_VERSION_MAJOR) "." \
	TO_STR2(RF_VERSION_MINOR) "." \
	TO_STR2(RF_VERSION_PATCHLEVEL)

struct redfish_version redfish_get_version(void)
{
	struct redfish_version version;

	version.major = RF_VERSION_MAJOR;
	version.minor = RF_VERSION_MINOR;
	version.patchlevel = RF_VERSION_PATCHLEVEL;
	return version;
}

const char* redfish_get_version_str(void)
{
	return RF_VERSION_STR;
}
