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

#include "stest/stest.h"

#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	struct redfish_mds_locator **mlocs;
	const char *user, *error;
	struct stest_custom_opt copt[] = {
		{
			.key = "error",
			.val = NULL,
			.help = "error=[0/1]\n    If 1, force an error\n",
		},
	};
	const int ncopt = sizeof(copt)/sizeof(copt[0]);

	stest_init(argc, argv, copt, ncopt, &user, &mlocs);
	stest_mlocs_free(mlocs);

	stest_set_status(10);
	error = copt_get("error", copt, ncopt);
	if (error && strcmp(error, "0"))
		stest_add_error("something went wrong!\n");

	return stest_finish();
}
