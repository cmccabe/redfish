/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "mon/action.h"
#include "mon/mon_info.h"
#include "util/string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

extern const struct mon_action write_file_test_act;
extern const struct mon_action ssh_test_act;
extern const struct mon_action install_act;
//extern struct mon_action *mds_status_act;
//extern struct mon_action *install_act;
//extern struct mon_action *start_act;
//extern struct mon_action *stop_act;
//extern struct mon_action *kill_act;
//extern struct mon_action *write_one_file_test;
//extern struct mon_action *round_trip_one_file_test;

static const struct mon_action *MON_ACTIONS[] = {
	&write_file_test_act,
	&ssh_test_act,
	&install_act,
//	&mds_status_act,
//	&install_act,
//	&start_act,
//	&stop_act,
//	&kill_act,
//	&write_one_file_test,
//	&round_trip_one_file_test,
};

#define NUM_MON_ACTIONS (sizeof(MON_ACTIONS)/sizeof(MON_ACTIONS[0]))

const char *get_mon_action_arg(struct action_arg **args,
			const char *name, const char *default_val)
{
	struct action_arg **a;

	for (a = args; *a; ++a) {
		if (strcmp((*a)->key, name) == 0) {
			return (*a)->val;
		}
	}
	return default_val;
}

static const char *mon_action_ty_to_str(enum mon_action_ty ty)
{
	switch(ty) {
	case MON_ACTION_ADMIN:
		return "administrative action";
	case MON_ACTION_IDLE:
		return "idle action";
	case MON_ACTION_TEST:
		return "test action";
	case MON_ACTION_UNIT_TEST:
		return "unit test";
	default:
		return "unknown";
	}
}

void print_action_descriptions(enum mon_action_ty ty)
{
	size_t i;
	fprintf(stderr, "%ss:\n", mon_action_ty_to_str(ty));
	for (i = 0; i < NUM_MON_ACTIONS; ++i) {
		if (MON_ACTIONS[i]->ty != ty)
			continue;
		print_lines(stderr, MON_ACTIONS[i]->desc);
	}
}

const struct mon_action* parse_one_action(const char *actname)
{
	size_t i;
	for (i = 0; i < NUM_MON_ACTIONS; ++i) {
		const struct mon_action *act = MON_ACTIONS[i];
		const char **n;
		for (n = act->names; *n; ++n) {
			if (strcmp(*n, actname) == 0) {
				return act;
			}
		}
	}
	return NULL;
}

/** Action arguments have the form FOO=BAR
 */
static struct action_arg** parse_action_args(char *err, size_t err_len,
					     char **argv, size_t *idx)
{
	struct action_arg** args = calloc(1, sizeof(struct action_arg*));
	if (!args)
		goto malloc_failed;
	while (1) {
		struct action_arg* a;
		char *eq, *str = argv[*idx];
		if (str == NULL)
			return args;
		eq = index(str, '=');
		if (eq == NULL)
			return args;
		a = JORM_ARRAY_APPEND_action_arg(&args);
		if (!a)
			goto malloc_failed;
		a->key = strdup(str);
		if (!a->key)
			goto malloc_failed;
		eq = index(a->key, '=');
		*eq = '\0';
		a->val = strdup(eq + 1);
		if (!a->val)
			goto malloc_failed;
		*idx = *idx + 1;
	}

malloc_failed:
	if (args)
		JORM_ARRAY_FREE_action_arg(&args);
	snprintf(err, err_len, "malloc failed");
	return NULL;
}

static void validate_action_args(char *err, size_t err_len,
	const struct mon_action *act, struct action_arg **args)
{
	if (args == NULL)
		return;
	for (; *args; ++args) {
		const char **a;
		for (a = act->args; *a; ++a) {
			if (strcmp(*a, (*args)->key) == 0) {
				break;
			}
		}
		if (*a == NULL) {
			snprintf(err, err_len, "Parse error: action "
				 "'%s' does not take '%s' as an argument.\n",
				 act->names[0], (*args)->key);
			return;
		}
	}
}

struct action_info** argv_to_action_info(char **argv, char *err, size_t err_len)
{
	size_t idx;
	struct action_info **arr = NULL;

	for (idx = 0; argv[idx]; ) {
		struct action_info *ai;
		const struct mon_action* act = parse_one_action(argv[idx]);
		if (!act) {
			snprintf(err, err_len, "There is no monitor action "
				 "named '%s'. Please enter a valid action.\n",
				 argv[idx]);
			JORM_ARRAY_FREE_action_info(&arr);
			return NULL;
		}
		ai = JORM_ARRAY_APPEND_action_info(&arr);
		if (!ai)
			goto malloc_failed;
		ai->act_name = strdup(act->names[0]);
		if (!ai->act_name)
			goto malloc_failed;
		++idx;
		ai->args = parse_action_args(err, err_len, argv, &idx);
		if (err[0])
			return NULL;
		validate_action_args(err, err_len, act, ai->args);
		if (err[0]) {
			JORM_ARRAY_FREE_action_info(&arr);
			return NULL;
		}
	}
	if ((arr == NULL) || (arr[0] == NULL)) {
		snprintf(err, err_len, "You must give at least one "
			 "action for fishmon to execute.");
		return NULL;
	}
	return arr;

malloc_failed:
	snprintf(err, err_len, "malloc failed.");
	JORM_ARRAY_FREE_action_info(&arr);
	return NULL;
}
