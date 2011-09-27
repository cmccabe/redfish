/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/fast_log.h"
#include "util/compiler.h"
#include "util/error.h"
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

static int dump_foo_bar_baz(struct fast_log_entry *f, int fd)
{
	struct foo_bar_baz_entry *fe = (struct foo_bar_baz_entry*)f;
	char buf[512];
	snprintf(buf, sizeof(buf), "foo=0x%"PRIx64 ", bar=0x%"PRIx64
		 ", bar=0x%" PRIx64 "\n", fe->foo, fe->bar, fe->baz);
	return safe_write(fd, buf, strlen(buf));
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

static int dump_gar(struct fast_log_entry *f, int fd)
{
	int ret;
	struct gar_entry *fe = (struct gar_entry*)f;
	ret = safe_write(fd, fe->gar, strlen(fe->gar));
	if (ret)
		return ret;
	return safe_write(fd, "\n", 1);
}

static const fast_log_dumper_fn_t g_dumpers[] = {
	[FASTLOG_TYPE_FOO_BAR_BAZ] = dump_foo_bar_baz,
	[FASTLOG_TYPE_GAR] = dump_gar,
};

/***** main *******/
static int dump_empty_buf(const char *tdir)
{
	char fname[PATH_MAX];
	char buf[512] = { 0 };
	const char expected[] = "*** FASTLOG empty\n";
	struct fast_log_buf *scratch, *empty;
	int res, fd;

	EXPECT_ZERO(zsnprintf(fname, sizeof(fname), "%s/empty", tdir));
	fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, 0644);
	EXPECT_GE(fd, 0);
	scratch = fast_log_create("scratch");
	EXPECT_NOT_EQUAL(scratch, NULL);
	empty = fast_log_create("empty");
	EXPECT_NOT_EQUAL(empty, NULL);
	EXPECT_ZERO(fast_log_dump(empty, scratch, fd));
	RETRY_ON_EINTR(res, close(fd));

	fast_log_destroy(empty);
	fast_log_destroy(scratch);
	EXPECT_GT(simple_io_read_whole_file_zt(fname, buf, sizeof(buf)), 0);
	EXPECT_ZERO(strcmp(buf, expected));
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
	struct fast_log_buf *scratch, *small;
	int res, fd;

	EXPECT_ZERO(zsnprintf(fname, sizeof(fname), "%s/small", tdir));
	fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, 0644);
	EXPECT_GE(fd, 0);
	scratch = fast_log_create("scratch");
	EXPECT_NOT_EQUAL(scratch, NULL);
	small = fast_log_create("small");
	EXPECT_NOT_EQUAL(small, NULL);
	create_some_logs(small, 1);
	EXPECT_ZERO(fast_log_dump(small, scratch, fd));
	RETRY_ON_EINTR(res, close(fd));

	fast_log_destroy(small);
	fast_log_destroy(scratch);
	EXPECT_GT(simple_io_read_whole_file_zt(fname, buf, sizeof(buf)), 0);
	EXPECT_ZERO(strcmp(buf, expected));
	return 0;
}

static int fill_entire_buf(const char *tdir)
{
	char fname[PATH_MAX];
	char buf[512] = { 0 };
	const char expected[] = "*** FASTLOG full\n\
foo=0x1, bar=0x2, bar=0x3\n";
	struct fast_log_buf *scratch, *full;
	int i, res, fd;
	struct foo_bar_baz_entry fe1 = {
		.type = FASTLOG_TYPE_FOO_BAR_BAZ,
		.foo = 0x1,
		.bar = 0x2,
		.baz = 0x3,
		.pad = { 0 }
	};

	EXPECT_ZERO(zsnprintf(fname, sizeof(fname), "%s/full", tdir));
	fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, 0644);
	EXPECT_GE(fd, 0);
	scratch = fast_log_create("scratch");
	EXPECT_NOT_EQUAL(scratch, NULL);
	full = fast_log_create("full");
	EXPECT_NOT_EQUAL(full, NULL);
	for (i = 0; i < 200000; ++i) {
		fast_log(full, &fe1);
	}
	EXPECT_ZERO(fast_log_dump(full, scratch, fd));
	RETRY_ON_EINTR(res, close(fd));
	fast_log_destroy(full);
	fast_log_destroy(scratch);
	EXPECT_GT(simple_io_read_whole_file_zt(fname, buf, sizeof(buf)), 0);
	EXPECT_ZERO(strncmp(buf, expected, strlen(expected)));
	return 0;
}

int main(void)
{
	char tdir[PATH_MAX];

	EXPECT_ZERO(get_tempdir(tdir, sizeof(tdir), 0775));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));
	EXPECT_ZERO(fast_log_init(g_dumpers));

	EXPECT_ZERO(dump_empty_buf(tdir));
	EXPECT_ZERO(dump_small_buf(tdir));
	EXPECT_ZERO(fill_entire_buf(tdir));

//	int res, fd1, fd2;
//	struct fast_log_buf *scratch, *fb1, *fb2;
//
//	scratch = fast_log_create("main");
//	EXPECT_NOT_EQUAL(scratch, NULL);
//	fb1 = fast_log_create("main");
//	EXPECT_NOT_EQUAL(fb1, NULL);
//	fb2 = fast_log_create("aux");
//	EXPECT_NOT_EQUAL(fb2, NULL);
//	fd0 = open_temp_file(tdir);
//	simple_fast_logs(fb1, 1);
//	simple_fast_logs(fb2, 2);
//	fd1 = open_temp_file(tdir);
//	EXPECT_GE(fd1, 0);
//	EXPECT_ZERO(fast_log_dump(fb1, scratch, fd1));
//	fd2 = open_temp_file(tdir);
//	EXPECT_GE(fd2, 0);
//	EXPECT_ZERO(fast_log_dump(fb2, scratch, fd2));
//	fast_log_destroy(scratch);
//	fast_log_destroy(fb1);
//	fast_log_destroy(fb2);
//	RETRY_ON_EINTR(res, close(fd0));
//	RETRY_ON_EINTR(res, close(fd1));
//	RETRY_ON_EINTR(res, close(fd2));

	return EXIT_SUCCESS;
}
