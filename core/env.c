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

#include "core/env.h"

#include <stdio.h>
#include <stdlib.h>

const char *getenv_or_die(const char *key)
{
	const char *val;

	val = getenv(key);
	if (!val) {
		fprintf(stderr, "You must set the environment variable "
			"%s for this program.\n", key);
		exit(EXIT_FAILURE);
	}
	return val;
}

const char *getenv_with_default(const char *key, const char *def)
{
	const char *val;

	val = getenv(key);
	if (!val)
		return def;
	return val;
}

