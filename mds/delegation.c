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

#include "mds/delegation.h"
#include "core/glitch_log.h"
#include "util/compiler.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int delegation_compare_dgid(const struct delegation *a,
		const struct delegation *b)
{
	uint64_t da, db;

	da = a->dgid;
	db = b->dgid;
	if (da < db)
		return -1;
	else if (da > db)
		return 1;
	else
		return 0;
}

void delegation_free(struct delegation *dg)
{
	free(dg);
}
