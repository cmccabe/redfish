/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/run_cmd.h"
#include "util/test.h"
#include "util/username.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	char buf[512], buf2[512], *newline;
	const char *cvec[] = { "whoami", NULL };

	EXPECT_EQUAL(get_current_username(buf, 0), -ENAMETOOLONG);
	EXPECT_ZERO(get_current_username(buf, sizeof(buf)));
	EXPECT_ZERO(run_cmd_get_output(buf2, sizeof(buf2), cvec));
	newline = index(buf2, '\n');
	if (newline)
		*newline = '\0';
	if (strcmp(buf, buf2)) {
		fprintf(stderr, "get_current_username returned '%s', but "
			"whoami returned '%s'\n", buf, buf2);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
