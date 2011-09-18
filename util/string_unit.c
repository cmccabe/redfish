/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/string.h"
#include "util/test.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int has_suffix_succeeded(const char *str, const char *suffix)
{
	if (has_suffix(str, suffix))
		return 0;
	else
		return 1;
}

static int has_suffix_failed(const char *str, const char *suffix)
{
	if (has_suffix(str, suffix))
		return 1;
	else
		return 0;
}

static char all_zero[16];

static int test_snappend(void)
{
	char buf[16];
	char canary[16];
	memset(buf, 0, sizeof(buf));
	memset(canary, 0, sizeof(canary));
	memset(all_zero, 0, sizeof(all_zero));
	snappend(buf, sizeof(buf), "abracadabrafoomanchucalifrag");
	EXPECT_ZERO(strcmp(buf, "abracadabrafoom"));
	EXPECT_ZERO(memcmp(canary, all_zero, sizeof(canary)));
	snappend(buf, sizeof(buf), "other stuff");
	EXPECT_ZERO(strcmp(buf, "abracadabrafoom"));
	EXPECT_ZERO(memcmp(canary, all_zero, sizeof(canary)));
	memset(buf, 0, sizeof(buf));
	snappend(buf, sizeof(buf), "%d", 123);
	EXPECT_ZERO(strcmp(buf, "123"));
	snappend(buf, sizeof(buf), "456");
	EXPECT_ZERO(strcmp(buf, "123456"));
	snappend(buf, sizeof(buf), "789");
	EXPECT_ZERO(strcmp(buf, "123456789"));
	return 0;
}

static int test_linearray_to_str(const char **lines, const char *expectstr)
{
	char *str = linearray_to_str(lines);
	if (!str)
		return -ENOMEM;
	if (strcmp(str, expectstr)) {
		free(str);
		return -EDOM;
	}
	free(str);
	return 0;
}

static const char *LTS_LINES1[] = {
	"abc",
	"123",
	"ggg",
	NULL
};

static const char *LTS_STR1 = "\
abc\n\
123\n\
ggg\n\
";

static const char *LTS_LINES2[] = {
	NULL
};

static const char *LTS_STR2 = "";

static const char *LTS_LINES3[] = {
	"a",
	NULL
};

static const char *LTS_STR3 = "a\n";

static int test_zsnprintf(size_t len, const char *str, int expect_succ)
{
	int ret;
	char buf[512];
	if (len > sizeof(buf)) {
		return -ENAMETOOLONG;
	}
	ret = zsnprintf(buf, len, "%s", str);
	if ((ret == 0) != (expect_succ != 0)) {
		fprintf(stderr, "test_zsnprintf(len=%Zd, str=%s, "
			"expect_succ=%d) failed\n",
			len, str, expect_succ);
		return 1;
	}
	return 0;
}

int main(void)
{
	EXPECT_ZERO(has_suffix_succeeded("abcd", "bcd"));
	EXPECT_ZERO(has_suffix_failed("", "bcd"));
	EXPECT_ZERO(has_suffix_failed("abcd", "abc"));
	EXPECT_ZERO(has_suffix_failed("ad", "ac"));
	EXPECT_ZERO(has_suffix_succeeded("zz", "zz"));
	EXPECT_ZERO(has_suffix_succeeded("long long long str", "str"));

	EXPECT_ZERO(test_linearray_to_str(LTS_LINES1, LTS_STR1));
	EXPECT_ZERO(test_linearray_to_str(LTS_LINES2, LTS_STR2));
	EXPECT_ZERO(test_linearray_to_str(LTS_LINES3, LTS_STR3));

	EXPECT_ZERO(test_snappend());

	EXPECT_ZERO(test_zsnprintf(1, "", 1));
	EXPECT_ZERO(test_zsnprintf(1, "1", 0));
	EXPECT_ZERO(test_zsnprintf(128, "foobar", 1));
	EXPECT_ZERO(test_zsnprintf(3, "ab", 1));
	EXPECT_ZERO(test_zsnprintf(3, "abc", 0));

	return EXIT_SUCCESS;
}
