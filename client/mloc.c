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

#include "client/fishc.h"
#include "util/str_to_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

struct redfish_mds_locator **redfish_mlocs_from_str(const char *str,
		char *err, size_t err_len)
{
	struct redfish_mds_locator **mlocs = NULL;
	char *buf = NULL, *state = NULL, *tok;

	mlocs = calloc(1, sizeof(struct redfish_mds_locator*));;
	if (!mlocs) {
		snprintf(err, err_len, "redfish_mlocs_from_str: OOM");
		goto error;
	}
	buf = strdup((str == NULL) ? "" : str);
	if (!buf) {
		snprintf(err, err_len, "redfish_mlocs_from_str: OOM");
		goto error;
	}
	for (tok = strtok_r(buf, ",", &state); tok;
		     (tok = strtok_r(NULL, ",", &state))) {
		redfish_mlocs_append(&mlocs, tok, err, err_len);
		if (err[0])
			goto error;
	}
	free(buf);
	return mlocs;

error:
	free(mlocs);
	free(buf);
	return NULL;
}

void redfish_mlocs_append(struct redfish_mds_locator ***mlocs, const char *s,
			 char *err, size_t err_len)
{
	int tn;
	struct redfish_mds_locator *zm = NULL, **m;
	char *colon;
	char err2[512] = { 0 };

	zm = calloc(1, sizeof(struct redfish_mds_locator));
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
	m = realloc(*mlocs, (tn + 2) * sizeof(struct redfish_mds_locator*));
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

static void redfish_mloc_free(struct redfish_mds_locator *mloc)
{
	free(mloc->host);
	free(mloc);
}

void redfish_mlocs_free(struct redfish_mds_locator **mlocs)
{
	struct redfish_mds_locator **m;

	for (m = mlocs; *m; ++m) {
		redfish_mloc_free(*m);
	}
	free(mlocs);
}

void redfish_free_block_locs(struct redfish_block_loc **blc)
{
	struct redfish_block_loc **i;

	i = blc;
	while (1) {
		int j;
		struct redfish_block_loc *b = *i;
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
