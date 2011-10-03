/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "stest/stest.h"

#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	struct redfish_mds_locator **mlocs;
	const char *user, *error;
	struct stest_custom_opt copt[] = {
		{
			.key = "error",
			.val = NULL,
			.help = "error=[0/1]\n    If 1, force an error\n",
		},
	};
	const int ncopt = sizeof(copt)/sizeof(copt[0]);

	stest_init(argc, argv, copt, ncopt, &user, &mlocs);
	stest_mlocs_free(mlocs);

	stest_set_status(10);
	error = copt_get("error", copt, ncopt);
	if (error && strcmp(error, "0"))
		stest_add_error("something went wrong!\n");

	return stest_finish();
}
