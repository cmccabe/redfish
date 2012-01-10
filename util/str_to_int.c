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

#include "util/str_to_int.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

void str_to_long_long(const char *str, int base, long long *ll,
	       char *err, size_t err_len)
{
	long long zll;
	char *endptr;

	/* This is one of the rare cases where we're supposed to change errno.
	 * See the man page for strtoll. */
	errno = 0;
	zll = strtoll(str, &endptr, base);
	if ((zll == LLONG_MIN) && (errno == ERANGE)) {
		snprintf(err, err_len, "underflow");
		return;
	}
	if ((zll == LLONG_MAX) && (errno == ERANGE)) {
		snprintf(err, err_len, "overflow");
		return;
	}
	if (str == endptr) {
		snprintf(err, err_len, "no digits found");
		return;
	}
	if (*endptr != '\0') {
		snprintf(err, err_len, "junk at end of string: %s", endptr);
		return;
	}
	*ll = zll;
}

void str_to_int(const char *str, int base, int *i,
	       char *err, size_t err_len)
{
	long long ll;
	/* There isn't any strtoi function in the standard library. And strtol
	 * doesn't help either, since sizeof(long int) == sizeof(long long) on
	 * 64-bit systems. So just use strtoll and check the range.
	 */
	str_to_long_long(str, base, &ll, err, err_len);
	if (err[0])
		return;
	if (ll > INT_MAX) {
		snprintf(err, err_len, "overflow");
		return;
	}
	if (ll < INT_MIN) {
		snprintf(err, err_len, "underflow");
		return;
	}
	*i = (int)ll;
}
