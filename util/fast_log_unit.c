/*
 * Copyright 2011-2012 the RedFish authors
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

#include "util/compiler.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_mgr.h"
#include "util/fast_log_types.h"
#include "util/macro.h"
#include "util/safe_io.h"
#include "util/simple_io.h"
#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

enum {
	FASTLOG_TYPE_FOO_BAR_BAZ = 1,
	FASTLOG_TYPE_GAR,
};

/***** foo_bar_baz_entry *******/
PACKED_ALIGNED(8,
struct foo_bar_baz_entry
{
	uint16_t type;
	uint64_t foo;
	uint64_t bar;
	uint64_t baz;
	char pad[6];
}
);
BUILD_BUG_ON(sizeof(struct foo_bar_baz_entry) !=
		sizeof(struct fast_log_entry));

static void dump_foo_bar_baz(struct fast_log_entry *f, char *buf)
{
	int ret;
	struct foo_bar_baz_entry *fe = (struct foo_bar_baz_entry*)f;
	ret = snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
		"foo=0x%"PRIx64 ", bar=0x%"PRIx64
		", bar=0x%" PRIx64 "\n", fe->foo, fe->bar, fe->baz);
}

/***** gar_entry *******/
PACKED_ALIGNED(8,
struct gar_entry
{
	uint16_t type;
	char gar[30];
}
);
BUILD_BUG_ON(sizeof(struct gar_entry) != sizeof(struct fast_log_entry));

static void dump_gar(struct fast_log_entry *f, char *buf)
{
	int ret;
	struct gar_entry *fe = (struct gar_entry*)f;
	ret = snprintf(buf, FAST_LOG_PRETTY_PRINTED_MAX,
		"%s\n", fe->gar);
}

static const fast_log_dumper_fn_t g_test_dumpers[] = {
	[FASTLOG_TYPE_FOO_BAR_BAZ] = dump_foo_bar_baz,
	[FASTLOG_TYPE_GAR] = dump_gar,
};

/***** main *******/
static int dump_empty_buf(const char *tdir)
{
	char fname[PATH_MAX];
	char buf[512] = { 0 };
	const char expected[] = "*** FASTLOG empty\n";
	struct fast_log_buf *empty;
	int res, fd;
	struct fast_log_mgr *mgr;

	mgr = fast_log_mgr_init(g_test_dumpers);
	EXPECT_NOT_ERRPTR(mgr);

	EXPECT_ZERO(zsnprintf(fname, sizeof(fname), "%s/empty", tdir));
	fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, 0644);
	EXPECT_GE(fd, 0);
	empty = fast_log_create(mgr, "empty");
	EXPECT_NOT_EQUAL(empty, NULL);
	EXPECT_ZERO(fast_log_dump(empty, g_test_dumpers, fd));
	RETRY_ON_EINTR(res, close(fd));

	fast_log_free(empty);
	EXPECT_GT(simple_io_read_whole_file_zt(fname, buf, sizeof(buf)), 0);
	EXPECT_ZERO(strcmp(buf, expected));
	fast_log_mgr_free(mgr);
	return 0;
}

static void create_some_logs(struct fast_log_buf *fb, int seed)
{
	struct foo_bar_baz_entry fe1 = {
		.type = FASTLOG_TYPE_FOO_BAR_BAZ,
		.foo = seed * 0x1,
		.bar = seed * 0x2,
		.baz = seed * 0x3,
		.pad = { 0 }
	};
	struct foo_bar_baz_entry fe2 = {
		.type = FASTLOG_TYPE_FOO_BAR_BAZ,
		.foo = seed * 0x10,
		.bar = seed * 0x20,
		.baz = seed * 0x30,
		.pad = { 0 }
	};
	struct gar_entry fe3;
	fe3.type = FASTLOG_TYPE_GAR;
	strncpy(fe3.gar, "blah blah", sizeof(fe3.gar));

	fast_log(fb, &fe1);
	fast_log(fb, &fe2);
	fast_log(fb, &fe3);
}

static int dump_small_buf(const char *tdir)
{
	char fname[PATH_MAX];
	char buf[512] = { 0 };
	const char expected[] = "*** FASTLOG small\n\
foo=0x1, bar=0x2, bar=0x3\n\
foo=0x10, bar=0x20, bar=0x30\n\
blah blah\n";
	struct fast_log_buf *small;
	int res, fd;
	struct fast_log_mgr *mgr;

	mgr = fast_log_mgr_init(g_test_dumpers);
	EXPECT_NOT_ERRPTR(mgr);
	EXPECT_ZERO(zsnprintf(fname, sizeof(fname), "%s/small", tdir));
	fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, 0644);
	EXPECT_GE(fd, 0);
	small = fast_log_create(mgr, "small");
	EXPECT_NOT_EQUAL(small, NULL);
	create_some_logs(small, 1);
	EXPECT_ZERO(fast_log_dump(small, g_test_dumpers, fd));
	RETRY_ON_EINTR(res, close(fd));

	fast_log_free(small);
	EXPECT_GT(simple_io_read_whole_file_zt(fname, buf, sizeof(buf)), 0);
	if (strcmp(buf, expected)) {
		fprintf(stderr, "got '%s', buf expected '%s'\n",
			buf, expected);
		return 1;
	}
	fast_log_mgr_free(mgr);
	return 0;
}

static int fill_entire_buf(const char *tdir)
{
	char fname[PATH_MAX];
	char buf[512] = { 0 };
	const char expected[] = "*** FASTLOG full\n\
foo=0x1, bar=0x2, bar=0x3\n";
	struct fast_log_buf *full;
	int i, res, fd;
	struct foo_bar_baz_entry fe1 = {
		.type = FASTLOG_TYPE_FOO_BAR_BAZ,
		.foo = 0x1,
		.bar = 0x2,
		.baz = 0x3,
		.pad = { 0 }
	};
	struct fast_log_mgr *mgr;

	mgr = fast_log_mgr_init(g_test_dumpers);
	EXPECT_NOT_ERRPTR(mgr);

	EXPECT_ZERO(zsnprintf(fname, sizeof(fname), "%s/full", tdir));
	fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, 0644);
	EXPECT_GE(fd, 0);
	full = fast_log_create(mgr, "full");
	EXPECT_NOT_EQUAL(full, NULL);
	for (i = 0; i < 200000; ++i) {
		fast_log(full, &fe1);
	}
	EXPECT_ZERO(fast_log_dump(full, g_test_dumpers, fd));
	RETRY_ON_EINTR(res, close(fd));
	fast_log_free(full);
	EXPECT_GT(simple_io_read_whole_file_zt(fname, buf, sizeof(buf)), 0);
	EXPECT_ZERO(strncmp(buf, expected, strlen(expected)));
	fast_log_mgr_free(mgr);
	return 0;
}

static int test_dump_all(const char *tdir)
{
	struct fast_log_mgr *mgr;
	char fname[PATH_MAX];
	char *expected, buf[4096] = { 0 };
	const char *expected_lines[] = {
		"*** FASTLOG one",
		"foo=0x1, bar=0x2, bar=0x3",
		"foo=0x10, bar=0x20, bar=0x30",
		"blah blah",
		"*** FASTLOG two",
		"foo=0x1, bar=0x2, bar=0x3",
		"foo=0x10, bar=0x20, bar=0x30",
		"blah blah",
		"*** FASTLOG three",
		"foo=0x1, bar=0x2, bar=0x3",
		"foo=0x10, bar=0x20, bar=0x30",
		"blah blah",
		NULL
	};
	struct fast_log_buf *one, *two, *three;
	int res, fd;

	mgr = fast_log_mgr_init(g_test_dumpers);
	EXPECT_NOT_ERRPTR(mgr);
	three = fast_log_create(mgr, "three");
	EXPECT_NOT_EQUAL(three, NULL);
	two = fast_log_create(mgr, "two");
	EXPECT_NOT_EQUAL(two, NULL);
	one = fast_log_create(mgr, "one");
	EXPECT_NOT_EQUAL(one, NULL);

	create_some_logs(one, 1);
	create_some_logs(two, 1);
	create_some_logs(three, 1);

	EXPECT_ZERO(zsnprintf(fname, sizeof(fname), "%s/all", tdir));
	fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, 0644);
	EXPECT_GE(fd, 0);
	EXPECT_ZERO(fast_log_mgr_dump_all(mgr, fd));
	RETRY_ON_EINTR(res, close(fd));

	fast_log_free(one);
	fast_log_free(two);
	fast_log_free(three);
	EXPECT_GT(simple_io_read_whole_file_zt(fname, buf, sizeof(buf)), 0);

	expected = linearray_to_str(expected_lines);
	if (strcmp(buf, expected)) {
		printf("buf = '%s', but expected = '%s'\n", buf, expected);
		return 1;
	}
	free(expected);
	fast_log_mgr_free(mgr);
	return 0;
}

static int test_tokenizer(void)
{
	char err[512] = { 0 };
	size_t err_len = sizeof(err);
	BITFIELD_DECL(bits, FAST_LOG_TYPE_MAX);

	memset(err, 0, err_len);
	str_to_fast_log_bitfield("MSGR_DEBUG;MSGR_DEBUG;MSGR_DEBUG", bits,
				 err, sizeof(err));
	EXPECT_ZERO(err[0]);

	memset(err, 0, err_len);
	str_to_fast_log_bitfield("MSGR_DEBUG;BOGUS;MSGR_DEBUG", bits,
				 err, sizeof(err));
	EXPECT_NONZERO(err[0]);

	memset(err, 0, err_len);
	str_to_fast_log_bitfield("MSGR_DEBUG;MSGR_DEBUG;MSGR_DEBUG;", bits,
				 err, sizeof(err));
	EXPECT_ZERO(err[0]);

	memset(err, 0, err_len);
	str_to_fast_log_bitfield(";", bits,
				 err, sizeof(err));
	EXPECT_ZERO(err[0]);

	memset(err, 0, err_len);
	str_to_fast_log_bitfield("", bits,
				 err, sizeof(err));
	EXPECT_ZERO(err[0]);

	return 0;
}

int main(void)
{
	char tdir[PATH_MAX];

	EXPECT_ZERO(get_tempdir(tdir, sizeof(tdir), 0775));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));

	EXPECT_ZERO(dump_empty_buf(tdir));
	EXPECT_ZERO(dump_small_buf(tdir));
	EXPECT_ZERO(fill_entire_buf(tdir));
	EXPECT_ZERO(test_dump_all(tdir));
	EXPECT_ZERO(test_tokenizer());

	return EXIT_SUCCESS;
}
