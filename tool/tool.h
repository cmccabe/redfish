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

#ifndef REDFISH_TOOL_MAIN_DOT_H
#define REDFISH_TOOL_MAIN_DOT_H

struct redfish_mds_locator;
struct fishtool_act;

#define NUM_LETTERS 26
#define MAX_NON_OPTION_ARGS 10

#define ALPHA_IDX(c) (int)((c >= 'a') ? (c - 'a') : (c - 'A'))

struct fishtool_params
{
	const struct fishtool_act *act;
	const char *cpath;
	const char *user_name;
	const char *lowercase_args[NUM_LETTERS];
	const char *uppercase_args[NUM_LETTERS];
	const char *non_option_args[MAX_NON_OPTION_ARGS];
};

struct fishtool_act
{
	const char *name;
	int (*fn)(struct fishtool_params *params);
	const char *getopt_str;
	const char **usage;
};

#endif
