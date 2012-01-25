/*
 * Copyright 2011-2012 the Redfish authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <leveldb/c.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

//PACKED(
//struct foo {
//	char a;
//	uint32_t b;
//	char c;
//}
//);

typedef int (*leveldb_test_fn_t)(leveldb_t *ldb);

static int run_leveldb_test(const char *tdir, leveldb_test_fn_t fn)
{
	char *err = NULL;
	char lname[PATH_MAX];
	leveldb_t *ldb;
	leveldb_options_t *lopt = NULL;

	EXPECT_ZERO(zsnprintf(lname, PATH_MAX, "%s/db", tdir));
	lopt = leveldb_options_create();
	EXPECT_NOT_EQ(lopt, NULL);
	leveldb_options_set_create_if_missing(lopt, 1);
	leveldb_options_set_compression(lopt, leveldb_no_compression);
	ldb = leveldb_open(lopt, lname, &err);
	if (err)
		goto ldb_error;
	if (fn) {
		EXPECT_ZERO(fn(ldb));
	}
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

static int reads_and_writes1(leveldb_t *ldb)
{
	char *val;
	size_t val_len;
	char *err = NULL;
	leveldb_writeoptions_t *lwopt;
	leveldb_readoptions_t *lropt;

	lwopt = leveldb_writeoptions_create();
	EXPECT_NOT_EQ(lwopt, NULL);
	leveldb_writeoptions_set_sync(lwopt, 1);
	leveldb_put(ldb, lwopt, "foo", strlen("foo"), "bar",
		strlen("bar"), &err);
	if (err)
		goto ldb_error;
	lropt = leveldb_readoptions_create();
	EXPECT_NOT_EQ(lropt, NULL);
	val = leveldb_get(ldb, lropt, "foo", strlen("foo"), &val_len, &err);
	if (err)
		goto ldb_error;
	EXPECT_EQ(val_len, strlen("bar"));
	EXPECT_ZERO(strncmp(val, "bar", strlen("bar")));
	free(val);

	leveldb_readoptions_destroy(lropt);
	leveldb_writeoptions_destroy(lwopt);
	return 0;

ldb_error:
	leveldb_writeoptions_destroy(lwopt);
	fprintf(stderr, "got ldb error: %s\n", err);
	free(err);
	return 1;
}

int main(void)
{
	char tdir[PATH_MAX];
	EXPECT_ZERO(get_tempdir(tdir, sizeof(tdir), 0755));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));
	EXPECT_ZERO(run_leveldb_test(tdir, NULL));
	EXPECT_ZERO(run_leveldb_test(tdir, reads_and_writes1));

	return EXIT_SUCCESS;
}
