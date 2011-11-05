/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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
	struct redfish_mds_locator **mlocs;
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
