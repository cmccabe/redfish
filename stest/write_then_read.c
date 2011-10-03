/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "client/fishc.h"
#include "stest/stest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STEST_FNAME "/test.txt"
#define SAMPLE_DATA "sample data!"
#define SAMPLE_DATA_LEN ((sizeof(SAMPLE_DATA) /  \
			  sizeof(SAMPLE_DATA[0])) - 1)

static int write_then_read(struct redfish_client *cli)
{
	struct redfish_file *ofe;
	char ibuf[SAMPLE_DATA_LEN] = SAMPLE_DATA;
	char obuf[SAMPLE_DATA_LEN] = { 0 };

	ofe = NULL;
	ST_EXPECT_ZERO(redfish_create(cli, STEST_FNAME, 0644, 0, 0, 0, &ofe));
	ST_EXPECT_ZERO(redfish_write(ofe, ibuf, SAMPLE_DATA_LEN));
	ST_EXPECT_ZERO(redfish_close(ofe));
	ofe = NULL;
	ST_EXPECT_ZERO(redfish_open(cli, STEST_FNAME, &ofe));
	ST_EXPECT_EQUAL(redfish_read(ofe, obuf, SAMPLE_DATA_LEN),
			SAMPLE_DATA_LEN);
	ST_EXPECT_ZERO(strcmp(ibuf, obuf));
	ST_EXPECT_ZERO(redfish_close(ofe));

	return 0;
}

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
		stest_mlocs_free(mlocs);
		return EXIT_FAILURE;
	}
	stest_mlocs_free(mlocs);

	write_then_read(cli);

	stest_set_status(10);
	error = copt_get("error", copt, ncopt);
	if (error && strcmp(error, "0")) {
		_exit(1);
	}
	redfish_disconnect(cli);
	return stest_finish();
}
