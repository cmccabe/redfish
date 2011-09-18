/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "mon/action.h"

#include <errno.h>
#include <stdio.h>

static const char *write_file_test_names[] = { "write_file_test", NULL };

static const char *write_file_test_desc[] = {
"Write some text to a file",
NULL
};

static const char *write_file_test_args[] = { "file_name", "text" , NULL };

static int do_write_file_test(const struct mon_action_args *args)
{
	int ret;
	FILE *fp;
	const char *file_name = get_mon_action_arg(args, "file_name", "/tmp/out");
	const char *text = get_mon_action_arg(args, "text", "sample_text");

	fp = fopen(file_name, "w");
	if (!fp) {
		ret = errno;
		return ret;
	}
	if (fputs(text, fp) < 0) {
		ret = errno;
		fclose(fp);
		return ret;
	}
	if (fclose(fp)) {
		ret = errno;
		return ret;
	}
	return 0;
}

const struct mon_action write_file_test_act = {
	.ty = MON_ACTION_UNIT_TEST,
	.names = write_file_test_names,
	.desc = write_file_test_desc,
	.args = write_file_test_args,
	.fn = do_write_file_test,
};
