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

#include "util/path.h"

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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
