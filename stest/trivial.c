/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "stest/stest.h"

#include <stdlib.h>

int main(int argc, char **argv)
{
	struct stest_custom_opt copt[] = {
		{
			.key = "error",
			.val = NULL,
			.help = "error=[0/1]     If 1, force an error",
		},
	};
	const int ncopt = sizeof(copt)/sizeof(copt[0]);

	stest_init(argc, argv, copt, ncopt);

	stest_set_status(10);
	if (copt_get("error", copt, ncopt) != NULL)
		stest_add_error("something went wrong!\n");

	return stest_finish();
}
