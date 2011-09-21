/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/compiler.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test that stdin matches the string that was supplied on the commandline.
 *
 * This executable is used to test run_cmd_get_output.
 */

static char* argv_to_buf(char **argv)
{
	char *str;
	char **a;
	size_t total_len = 0;
	for (a = argv; *a; ++a) {
		total_len += strlen(*a);
	}
	total_len++;
	str = malloc(total_len * sizeof(char));
	if (!str)
		return NULL;
	str[0] = '\0';
	for (a = argv; *a; ++a) {
		strcat(str, *a);
	}
	return str;
}

#define CHUNK_SZ 512

static char* fp_to_buf(FILE *fp)
{
	char *str = NULL;
	int nchunks = 1;

	while (1) {
		int res;
		char *s, *snext, buf[CHUNK_SZ];
		s = realloc(str, 1 + (CHUNK_SZ * nchunks));
		if (!s) {
			fprintf(fp, "out of memory.\n");
			free(str);
			return NULL;
		}
		str = s;
		snext = str + (CHUNK_SZ * (nchunks - 1));
		res = fread(buf, 1, CHUNK_SZ, fp);
		if (res < CHUNK_SZ) {
			if (res < 0) {
				fprintf(fp, "fread error %d\n", ferror(fp));
				free(str);
				return NULL;
			}
			memcpy(snext, buf, res);
			snext[res] = '\0';
			return str;
		}
		memcpy(snext, buf, CHUNK_SZ);
		nchunks++;
	}
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	int ret;
	char *abuf, *ibuf;

	abuf = argv_to_buf(argv + 1);
	if (!abuf) {
		ret = EXIT_FAILURE;
		goto done;
	}

	ibuf = fp_to_buf(stdin);
	if (!ibuf) {
		fprintf(stderr, "failed to read stdin\n");
		ret = EXIT_FAILURE;
		goto done_free_abuf;
	}

	if (strcmp(abuf, ibuf)) {
		fprintf(stderr, "error: expected: '%s'\ngot: '%s'\n", abuf, ibuf);
		fprintf(stderr, "len(expected) = %Zd, len(got) = %Zd\n",
			strlen(abuf), strlen(ibuf));
		ret = EXIT_FAILURE;
		goto done_free_ibuf;
	}
	ret = EXIT_SUCCESS;

done_free_ibuf:
	free(ibuf);
done_free_abuf:
	free(abuf);
done:
	return ret;
}
