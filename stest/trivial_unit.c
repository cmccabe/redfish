/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/compiler.h"
#include "util/run_cmd.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	char st_trivial[PATH_MAX];

	EXPECT_ZERO(get_colocated_path(argv[0], "st_trivial",
			      st_trivial, sizeof(st_trivial)));
	EXPECT_ZERO(run_cmd(st_trivial, "-h", (char*)NULL));
	EXPECT_ZERO(run_cmd(st_trivial, "-f", (char*)NULL));
	EXPECT_ZERO(run_cmd(st_trivial, "-f", "error=0", (char*)NULL));
	EXPECT_NONZERO(run_cmd(st_trivial, "-f", "error=1", (char*)NULL));
	EXPECT_ZERO(run_cmd(st_trivial, "-m", "localhost:8080",
			"-m", "foobarhost:8081", "-u", "myuser",
			"-f", "error=0", (char*)NULL));
	return 0;
}
