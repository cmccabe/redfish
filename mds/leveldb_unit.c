/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <leveldb/c.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

//PACKED(
//struct foo {
//	char a;
//	uint32_t b;
//	char c;
//}
//);

static int test1(const char *tdir)
{
	char *err;
	char lname[PATH_MAX];
	leveldb_t *ldb;
	leveldb_options_t *lopt = NULL;

	EXPECT_ZERO(zsnprintf(lname, PATH_MAX, "%s/db", tdir));
	lopt = leveldb_options_create();
	EXPECT_NOT_EQUAL(lopt, NULL);
	leveldb_options_set_create_if_missing(lopt, 1);
	leveldb_options_set_compression(lopt, leveldb_no_compression);
	ldb = leveldb_open(lopt, lname, &err);
	if (err)
		goto ldb_error;
	leveldb_close(ldb);
	leveldb_destroy_db(lopt, lname, &err);
	if (err)
		goto ldb_error;
	leveldb_options_destroy(lopt);
	return 0;

ldb_error:
	leveldb_options_destroy(lopt);
	fprintf(stderr, "got ldb error: %s\n", err);
	free(err);
	return 1;
}

int main(void)
{
	char tdir[PATH_MAX];
	EXPECT_ZERO(get_tempdir(tdir, sizeof(tdir), 0755));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));
	EXPECT_ZERO(test1(tdir));

	return EXIT_SUCCESS;
}
