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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int has_suffix(const char *str, const char *suffix)
{
	size_t str_len = strlen(str);
	size_t suffix_len = strlen(suffix);
	if (suffix_len > str_len)
		return 0;
	if (strcmp(str + (str_len - suffix_len), suffix) == 0)
		return 1;
	else
		return 0;
}

void snappend(char *str, size_t str_len, const char *fmt, ...)
{
	va_list ap;
	size_t slen = strlen(str);
	if (slen >= str_len + 1)
		return;
	va_start(ap, fmt);
	vsnprintf(str + slen, str_len - slen, fmt, ap);
	va_end(ap);
}

int zsnprintf(char *out, size_t out_len, const char *fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vsnprintf(out, out_len, fmt, ap);
	va_end(ap);
	if (ret >= (int)out_len)
		return -ENAMETOOLONG;
	if (ret < 0)
		return ret;
	return 0;
}

char *linearray_to_str(const char **lines)
{
	char *b, *ret;
	size_t total_size = 0;
	const char **l;
	for (l = lines; *l; ++l) {
		total_size += strlen(*l) + 1;
	}
	ret = malloc(total_size + 1);
	if (!ret)
		return ret;
	b = ret;
	for (l = lines; *l; ++l) {
		strcpy(b, *l);
		b += strlen(*l);
		b[0] = '\n';
		b++;
	}
	b[0] = '\0';
	return ret;
}

void write_linearray_to_file(const char *file_name, const char **lines,
				char *err, size_t err_len)
{
	FILE *fp;
	const char **l;

	fp = fopen(file_name, "w");
	if (!fp) {
		int ret = errno;
		snprintf(err, err_len, "error opening '%s' for writing: %d\n",
			 file_name, ret);
		return;
	}

	for (l = lines; *l; ++l) {
		if (fprintf(fp, "%s\n", *l) < 0) {
			snprintf(err, err_len, "error writing to '%s'\n",
				 file_name);
			fclose(fp);
			return;
		}
	}
	if (fclose(fp)) {
		int ret = errno;
		snprintf(err, err_len, "error closing '%s' for writing: %d\n",
			 file_name, ret);
		return;
	}
}

void print_lines(FILE *fp, const char **lines)
{
	const char **l;
	for (l = lines; *l; ++l) {
		fprintf(fp, "%s\n", *l);
	}
}

uint32_t ohash_str(const char *str)
{
	uint32_t h = 5381;
	while (1) {
		char c = *str++;
		if (c == '\0')
			return h;
		h = ((h << 5) + h) + (c);
	}
}

static int hexify(int num)
{
	if ((num >= 0) && (num <= 9))
		return '0' + num;
	else if ((num >= 10) && (num <= 16))
		return 'a' + (num - 10);
	else
		return 'X';
}

void hex_dump(const char *buf, size_t buf_len, char *str, size_t str_len)
{
	int val;
	int mod8;

	if (str_len < 4) {
		snprintf(str, str_len, "...");
		return;
	}
	mod8 = 0;
	while (1) {
		if (buf_len == 0)
			break;
		val = *((unsigned char*)buf);
		buf++;
		buf_len--;
		if (str_len < 4)
			break;
		*str++ = hexify(val >> 4);
		*str++ = hexify(val & 0xf);
		if (mod8 == 7) {
			*str++ = '\n';
			mod8 = 0;
		}
		else {
			*str++ = ' ';
			mod8++;
		}
		str_len -= 3;
	}
	if (buf_len > 0) {
		*str++ = '.';
		*str++ = '.';
		*str++ = '.';
	}
	*str++ = '\0';
}

void fwdprintf(char *buf, size_t *off, size_t buf_len, const char *fmt, ...)
{
	int res;
	size_t o;
	va_list ap;

	o = *off;
	va_start(ap, fmt);
	res = vsnprintf(buf + o, buf_len - o, fmt, ap);
	va_end(ap);
	if (res < 0)
		return;
	else if (o + res > buf_len)
		*off = buf_len;
	else
		*off = o + res;
}

char* strdupcat(const char *a, const char *b)
{
	char *str;
	size_t la = strlen(a);
	size_t lb = strlen(b);

	str = malloc(la + lb + 1);
	if (!str)
		return NULL;
	sprintf(str, "%s%s", a, b);
	return str;
}
