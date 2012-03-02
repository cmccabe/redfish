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

#include "util/path.h"
#include "util/string.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int canonicalize_path(char *src)
{
	char *dst = src;
	int saw_non_slash = 0, last_was_slash = 0;

	if (src[0] != '/')
		return -ENOTSUP;

	while (1) {
		char c = *src++;
		if (c == '\0') {
			if (saw_non_slash && last_was_slash)
				dst--;
			break;
		}
		else if (c == '/') {
			if (!last_was_slash)
				*dst++ = c;
			last_was_slash = 1;
		}
		else {
			*dst++ = c;
			last_was_slash = 0;
			saw_non_slash = 1;
		}
	}
	*dst = '\0';
	return 0;
}

int canonicalize_path2(char *dst, size_t dst_len, const char *src)
{
	int saw_non_slash = 0, last_was_slash = 0;
	size_t cnt = 0;
	char c;

	if (src[0] != '/')
		return -ENOTSUP;

	while (1) {
		if (++cnt >= dst_len)
			return -ENAMETOOLONG;
		c = *src++;
		if (c == '\0') {
			if (saw_non_slash && last_was_slash)
				dst--;
			break;
		}
		else if (c == '/') {
			if (!last_was_slash)
				*dst++ = c;
			last_was_slash = 1;
		}
		else {
			*dst++ = c;
			last_was_slash = 0;
			saw_non_slash = 1;
		}
	}
	*dst = '\0';
	return cnt - 1;
}

int canon_path_append(char *base, size_t out_len, const char *suffix)
{
	size_t cbase_len;

	if (!suffix[0])
		return 0;
	cbase_len = strlen(base);
	if (cbase_len < 2)
		return zsnprintf(base, out_len, "/%s", suffix);
	return zsnprintf(base + cbase_len, out_len - cbase_len, "/%s", suffix);
}

int canon_path_add_suffix(char *base, size_t out_len, char suffix)
{
	size_t cbase_len;

	cbase_len = strlen(base);
	if (cbase_len < 2)
		return zsnprintf(base, out_len, "%c", suffix);
	return zsnprintf(base + cbase_len, out_len - cbase_len, "%c", suffix);
}

void do_dirname(const char *fname, char *dname, size_t dname_len)
{
	char *ls;

	snprintf(dname, dname_len, "%s", fname);
	ls = rindex(dname, '/');
	if (!ls) {
		/* If there were no slashes, we're looking at a relative path,
		 * and the directory is the local directory. */
		snprintf(dname, dname_len, ".");
		return;
	}
	if (ls == dname) {
		/* Special case: the dirname of / is itself. */
		return;
	}
	*ls = '\0';
}
