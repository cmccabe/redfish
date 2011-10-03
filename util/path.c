/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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
