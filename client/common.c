/*
 * Copyright 2012 the Redfish authors
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

#include <stdlib.h>
#include <stdio.h>

/* "Convenience" functions that are the same in any client implementation. */
void redfish_disconnect_and_release(struct redfish_client *cli)
{
	redfish_disconnect(cli);
	redfish_release_client(cli);
}

int redfish_close_and_free(struct redfish_file *ofe)
{
	int ret;

	ret = redfish_close(ofe);
	redfish_free_file(ofe);
	return ret;
}

void redfish_free_path_status(struct redfish_stat* osa)
{
	free(osa->path);
	free(osa->owner);
	free(osa->group);
}

void redfish_free_path_statuses(struct redfish_stat* osas, int nosa)
{
	int i;

	for (i = 0; i < nosa; ++i) {
		redfish_free_path_status(osas + i);
	}
	free(osas);
}

void redfish_free_block_locs(struct redfish_block_loc **blcs, int nblc)
{
	struct redfish_block_loc *blc;
	int i, j;

	for (i = 0; i < nblc; ++i) {
		blc = blcs[i];
		for (j = 0; j < blc->nhosts; ++j) {
			free(blc->hosts[j].hostname);
		}
		free(blc);
	}
	free(blcs);
}
