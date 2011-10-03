/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/compiler.h"
#include "util/run_cmd.h"
#include "util/simple_io.h"
#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FISHMON_WFT_ARGS "-f"

static const char *CONF[] = {
	"{",
	"	\"base_dir\" : \"%s\", # test out the commenting system!",
	"	\"default\" : {",
	"		\"base_dir\" : \"%s\",",
	"		\"foo_bar_baz\" : \"#string with hash mark inside\"",
	"	},",
	"	\"cluster\" : {",
	"               \"daemons\": [",
	"			{",
	"				\"type\" : \"osd\",",
	"				\"config\" : \"/etc/osd.conf\"",
	"			},",
	"			{",
	"				\"type\" : \"mds\",",
	"				\"config\" : \"/etc/mds.conf\"",
	"			}",
	"		]",
	"	}",
	"}",
	NULL
};

static int write_file_test1(const char *fishmon, const char *conf,
			    const char *tdir)
{
	char farg[PATH_MAX], fname[PATH_MAX], buf[512] = { 0 };
	EXPECT_ZERO(zsnprintf(fname, sizeof(farg), "%s/test1.txt", tdir));
	EXPECT_ZERO(zsnprintf(farg, sizeof(farg), "file_name=%s", fname));
	EXPECT_ZERO(run_cmd(fishmon, "-c", conf, FISHMON_WFT_ARGS,
			    "write_file_test", farg, (char*)NULL));
	EXPECT_POSITIVE(simple_io_read_whole_file_zt(fname, buf, sizeof(buf)));
	EXPECT_ZERO(strcmp(buf, "sample_text"));
	return 0;
}

static int write_file_test2(const char *fishmon, const char *conf,
			    const char *tdir)
{
	char farg[PATH_MAX], fname[PATH_MAX], buf[512] = { 0 };
	EXPECT_ZERO(zsnprintf(fname, sizeof(farg), "%s/test2.txt", tdir));
	EXPECT_ZERO(zsnprintf(farg, sizeof(farg), "file_name=%s", fname));
	EXPECT_ZERO(run_cmd(fishmon, "-c", conf, FISHMON_WFT_ARGS,
		"write_file_test", farg, "text=foobar", (char*)NULL));
	EXPECT_POSITIVE(simple_io_read_whole_file_zt(fname, buf, sizeof(buf)));
	EXPECT_ZERO(strcmp(buf, "foobar"));
	return 0;
}

static int write_file_test3(const char *fishmon, const char *conf,
			    const char *tdir)
{
	char fname1[PATH_MAX], fname2[PATH_MAX];
	char farg1[PATH_MAX], farg2[PATH_MAX];
	char buf[512] = { 0 };
	EXPECT_ZERO(zsnprintf(fname1, sizeof(farg1), "%s/test3.1.txt", tdir));
	EXPECT_ZERO(zsnprintf(fname2, sizeof(farg2), "%s/test3.2.txt", tdir));
	EXPECT_ZERO(zsnprintf(farg1, sizeof(farg1), "file_name=%s", fname1));
	EXPECT_ZERO(zsnprintf(farg2, sizeof(farg2), "file_name=%s", fname2));
	EXPECT_ZERO(run_cmd(fishmon, "-c", conf, FISHMON_WFT_ARGS,
		"write_file_test", farg1, "text=farg1",
		"write_file_test", farg2, "text=farg2", (char*)NULL));
	EXPECT_POSITIVE(simple_io_read_whole_file_zt(fname1, buf, sizeof(buf)));
	EXPECT_ZERO(strcmp(buf, "farg1"));
	EXPECT_POSITIVE(simple_io_read_whole_file_zt(fname2, buf, sizeof(buf)));
	EXPECT_ZERO(strcmp(buf, "farg2"));
	return 0;
}

static int write_file_test4(const char *fishmon, const char *conf,
			    const char *tdir)
{
	char farg[PATH_MAX], fname[PATH_MAX];
	EXPECT_ZERO(zsnprintf(fname, sizeof(farg), "%s/test2.txt", tdir));
	EXPECT_ZERO(zsnprintf(farg, sizeof(farg), "file_name=%s", fname));
	/* unknown arguments should cause an error. */
	EXPECT_NONZERO(run_cmd(fishmon, "-c", conf, FISHMON_WFT_ARGS,
		"write_file_test", farg, "text=foobar", "unknown=yep",
		(char*)NULL));
	return 0;
}

static int make_temp_conf_file(const char *tdir, char *conf, size_t conf_len)
{
	FILE *fp;
	char *cstr;
	cstr = linearray_to_str(CONF);
	if (!cstr) {
		fprintf(stderr, "linearray_to_str: out of memory\n");
		return -ENOMEM;
	}
	snprintf(conf, conf_len, "%s/mon.temp.conf", tdir);
	fp = fopen(conf, "w");
	if (!fp) {
		int ret = errno;
		free(cstr);
		fprintf(stderr, "error opening %s: %d\n", conf, ret);
		return -EIO;
	}
	fprintf(fp, cstr, tdir, tdir);
	free(cstr);
	fclose(fp);
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	char fishmon[PATH_MAX], conf[PATH_MAX];
	char tdir[PATH_MAX];
	EXPECT_ZERO(get_tempdir(tdir, sizeof(tdir), 0700));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));

	EXPECT_ZERO(get_colocated_path(argv[0], "fishmon",
			      fishmon, sizeof(fishmon)));
	EXPECT_ZERO(make_temp_conf_file(tdir, conf, sizeof(conf)));

	EXPECT_EQUAL(run_cmd(fishmon, (char*)NULL), 1);
	EXPECT_ZERO(run_cmd(fishmon, "-c", conf, "-h", (char*)NULL));
	EXPECT_ZERO(run_cmd(fishmon, "-c", conf, "-T", (char*)NULL));
	EXPECT_ZERO(write_file_test1(fishmon, conf, tdir));
	EXPECT_ZERO(write_file_test2(fishmon, conf, tdir));
	EXPECT_ZERO(write_file_test3(fishmon, conf, tdir));
	EXPECT_ZERO(write_file_test4(fishmon, conf, tdir));

	return 0;
}
