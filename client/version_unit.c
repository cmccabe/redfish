/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "client/fishc.h"
#include "util/test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	const char *version_str;
	struct redfish_version version;

	version = redfish_get_version();
	EXPECT_GT(version.major, 0);
	version_str = redfish_get_version_str();
	if (strspn(version_str, "0123456789.") != strlen(version_str)) {
		fprintf(stderr, "unexpected characters in version string!  "
			"str = '%s'\n", version_str);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
