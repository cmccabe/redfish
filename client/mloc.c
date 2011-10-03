/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * * This is licensed under the Apache License, Version 2.0.
 */

#include "client/fishc.h"
#include "util/str_to_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void onefish_mlocs_append(struct of_mds_locator ***mlocs, const char *s,
			 char *err, size_t err_len)
{
	int tn;
	struct of_mds_locator *zm = NULL, **m;
	char *colon;
	char err2[512] = { 0 };

	zm = calloc(1, sizeof(struct of_mds_locator));
	if (!zm)
		goto oom;
	zm->host = strdup(s);
	if (!zm->host)
		goto oom;
	colon = rindex(zm->host, ':');
	if (!colon) {
		snprintf(err, err_len, "error parsing metadata server "
			 "locator '%s': couldn't find colon!\nMetadata "
			 "server locators must have the format "
			 "hostname:port\n", s);
		goto error;
	}
	*colon = '\0';
	str_to_int(colon + 1, 10, &zm->port, err2, sizeof(err2));
	if (err2[0]) {
		snprintf(err, err_len, "error parsing metadata server "
			 "locator '%s': couldn't parse port as a "
			 "number! error: '%s'\nMetadata server "
			"locators must have the format "
			"hostname:port\n", s, err2);
		goto error;
	}
	for (tn = 0, m = *mlocs; *m; ++m) {
		tn++;
	}
	m = realloc(*mlocs, (tn + 2) * sizeof(struct of_mds_locator*));
	if (!m)
		goto oom;
	*mlocs = m;
	m[tn] = zm;
	m[tn + 1] = NULL;
	return;
oom:
	snprintf(err, err_len, "mlocs_append: out of memory\n");
error:
	free(zm->host);
	free(zm);
}

void onefish_free_block_locs(struct of_block_loc **blc)
{
	struct of_block_loc **i;

	i = blc;
	while (1) {
		int j;
		struct of_block_loc *b = *i;
		if (!b)
			break;
		for (j = 0; j < b->num_hosts; ++j) {
			free(b->hosts[j].hostname);
		}
		free(b);
		++i;
	}
	free(blc);
}
