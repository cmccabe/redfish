/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "client/fishc.h"
#include "stest/stest.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int ret;
	struct redfish_client *cli = NULL;
	struct redfish_mds_locator **mlocs;
	const char *user, *error;
	struct stest_custom_opt copt[] = {
		{
			.key = "crash",
			.val = NULL,
			.help = "error=[0/1]\n"
				"If 1, crash without properly disconnecting\n",
		},
	};
	const int ncopt = sizeof(copt)/sizeof(copt[0]);

	stest_init(argc, argv, copt, ncopt, &user, &mlocs);
	ret = redfish_connect(mlocs, user, &cli);
	if (ret) {
		stest_add_error("redfish_connect: failed to connect: "
				"error %d\n", ret);
	}
	stest_mlocs_free(mlocs);

	stest_set_status(10);
	error = copt_get("error", copt, ncopt);

	if (error && strcmp(error, "0")) {
		_exit(1);
	}
	if (cli) {
		redfish_disconnect(cli);
	}

	return stest_finish();
}
