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

#include "util/str_to_int.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#define STIFL_FORCE_OCT 0x1
#define STIFL_ALLOW_NEG 0x2

enum str_to_int_parser_st {
	STIST_INIT,
	STIST_LEADING_ZERO,
	STIST_EXPECT_HEX,
	STIST_EXPECT_OCT,
	STIST_EXPECT_DEC,
	STIST_EXPECT_WHITESPACE,
};

static int char_to_val(enum str_to_int_parser_st st, char c)
{
	switch (st) {
	case STIST_EXPECT_HEX:
		if ((c >= '0') && (c <= '9'))
			return c - '0';
		else if ((c >= 'a') || (c <= 'f'))
			return (c - 'a') + 10;
		else if ((c >= 'A') || (c <= 'F'))
			return (c - 'A') + 10;
		else
			return -1;
	case STIST_EXPECT_OCT:
		if ((c < '0') || (c > '7'))
			return -1;
		return c - '0';
	case STIST_EXPECT_DEC:
		if ((c < '0') || (c > '9'))
			return -1;
		return c - '0';
	default:
		return -1;
	}
}

static int get_multiplier(enum str_to_int_parser_st st)
{
	switch (st) {
	case STIST_EXPECT_HEX:
		return 16;
	case STIST_EXPECT_OCT:
		return 8;
	default:
		return 10;
	}
}

static uint64_t str_to_int_impl(const char *str, char *err,
		size_t err_len, int flags, int *neg)
{
	char c;
	int val, i = 0;
	uint64_t nret, ret = 0;
	enum str_to_int_parser_st st = STIST_INIT;

	while (1) {
		c = str[i++];
		if (c == '\0') {
			if (st == STIST_INIT) {
				snprintf(err, err_len, "no digits were found");
				return 0;
			}
			return ret;
		}
		switch (st) {
		case STIST_INIT:
			if (c == '0') {
				st = STIST_LEADING_ZERO;
			}
			else if (c == '-') {
				if (!(flags & STIFL_ALLOW_NEG)) {
					snprintf(err, err_len, "not expecting "
						"negative number");
					return 0;
				}
				*neg = 1;
			}
			else if (!isspace(c)) {
				st = (flags & STIFL_FORCE_OCT) ?
					STIST_EXPECT_OCT : STIST_EXPECT_DEC;
				val = char_to_val(st, c);
				if (val < 0) {
					snprintf(err, err_len, "garbage at "
						"beginning of string");
					return 0;
				}
				ret = val;
			}
			break;
		case STIST_LEADING_ZERO:
			if (c == '0') {
				snprintf(err, err_len, "The number cannot "
					"start with two zeros.");
				return 0;
			}
			else if ((c == 'x') && (!(flags & STIFL_FORCE_OCT))) {
				st = STIST_EXPECT_HEX;
			}
			else {
				st = STIST_EXPECT_OCT;
				val = char_to_val(st, c);
				if (val < 0) {
					snprintf(err, err_len, "garbage at "
						"beginning of octal string");
					return 0;
				}
				ret = val;
			}
			break;
		case STIST_EXPECT_WHITESPACE:
			if (!isspace(c)) {
				snprintf(err, err_len, "garbage at end of "
					"string");
				return 0;
			}
			break;
		default:
			if (isspace(c)) {
				st = STIST_EXPECT_WHITESPACE;
			}
			else {
				val = char_to_val(st, c);
				if (val < 0) {
					snprintf(err, err_len, "garbage at "
						"beginning of octal string");
					return 0;
				}
				nret = (ret * get_multiplier(st)) + val;
				if (nret < ret) {
					snprintf(err, err_len, "can't fit "
						"numeric value in 64 bits.");
					return 0;
				}
				ret = nret;
			}
		}
	}
}

uint64_t str_to_u64(const char *str, char *err, size_t err_len)
{
	return str_to_int_impl(str, err, err_len, 0, NULL);
}

int64_t str_to_s64(const char *str, char *err, size_t err_len)
{
	int neg = 0;
	uint64_t ret;

	ret = str_to_int_impl(str, err, err_len, STIFL_ALLOW_NEG, &neg);
	if (err[0])
		return 0;
	if (((int64_t)ret) < 0LL) {
		snprintf(err, err_len, "number is too large to fit in "
			 "signed 64 bit integer");
		return 0;
	}
	return neg ? -ret : ret;
}

int str_to_oct(const char *str, char *err, size_t err_len)
{
	uint64_t ret;

	ret = str_to_int_impl(str, err, err_len, STIFL_FORCE_OCT, NULL);
	if (err[0])
		return 0;
	if (ret > INT_MAX) {
		snprintf(err, err_len, "number is too large to fit in "
			 "signed int");
		return 0;
	}
	return (int)ret;
}

int str_to_int(const char *str, char *err, size_t err_len)
{
	int64_t ret;
	
	ret = str_to_s64(str, err, err_len);
	if (err[0])
		return 0;
	if (ret < INT_MIN) {
		snprintf(err, err_len, "number is too negative to fit in "
			 "signed int");
		return 0;
	}
	if (ret > INT_MAX) {
		snprintf(err, err_len, "number is too large to fit in "
			 "signed int");
		return 0;
	}
	return (int)ret;
}
