/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "mon/action.h"
#include "mon/mon_info.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static const char *write_file_test_names[] = { "write_file_test", NULL };

static const char *write_file_test_desc[] = {
"write_file_test: write some text to a file",
NULL
};

static const char *write_file_test_args[] = { "file_name", "text" , NULL };

static int do_write_file_test(struct action_info *ai,
			      struct action_arg ** args)
{
	int ret;
	FILE *fp;
	const char *file_name, *text;

	file_name = get_mon_action_arg(args, "file_name", "/tmp/out");
	text = get_mon_action_arg(args, "text", "sample_text");

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
	pthread_mutex_lock(&g_mon_info_lock);
	ai->percent_done = 100;
	ai->changed = 1;
	pthread_mutex_unlock(&g_mon_info_lock);
	return 0;
}

const struct mon_action write_file_test_act = {
	.ty = MON_ACTION_UNIT_TEST,
	.names = write_file_test_names,
	.desc = write_file_test_desc,
	.args = write_file_test_args,
	.fn = do_write_file_test,
};
