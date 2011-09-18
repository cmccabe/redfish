/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "mon/action.h"
#include "util/string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

extern const struct mon_action write_file_test_act;
//extern struct mon_action *ssh_status_act;
//extern struct mon_action *mds_status_act;
//extern struct mon_action *install_act;
//extern struct mon_action *start_act;
//extern struct mon_action *stop_act;
//extern struct mon_action *kill_act;
//extern struct mon_action *write_one_file_test;
//extern struct mon_action *round_trip_one_file_test;

static const struct mon_action *MON_ACTIONS[] = {
	&write_file_test_act,
	//&status_act,
//	&install_act,
//	&start_act,
//	&stop_act,
//	&kill_act,
//	&write_one_file_test,
//	&round_trip_one_file_test,
};

#define NUM_MON_ACTIONS (sizeof(MON_ACTIONS)/sizeof(MON_ACTIONS[0]))

const char *get_mon_action_arg(const struct mon_action_args *arglist,
			const char *name, const char *default_val)
{
	char **n = arglist->name;
	char **v = arglist->val;
	for (; *n; ++n, ++v) {
		if (strcmp(*n, name) == 0) {
			return *v;
		}
	}
	return default_val;
}

void free_mon_action_args(struct mon_action_args* args)
{
	if (!args)
		return;
	if (args->name) {
		size_t i = 0;
		while (1) {
			if (args->name[i])
				free(args->name[i++]);
			else
				break;
		}
		free(args->name);
	}
	if (args->val) {
		size_t i = 0;
		while (1) {
			if (args->val[i])
				free(args->val[i++]);
			else
				break;
		}
		free(args->val);
	}
	free(args);
}

void print_action_descriptions(enum mon_action_ty ty)
{
	size_t i;
	for (i = 0; i < NUM_MON_ACTIONS; ++i) {
		if (MON_ACTIONS[i]->ty != ty)
			continue;
		print_lines(stderr, MON_ACTIONS[i]->desc);
	}
}

static const struct mon_action* parse_one_action(const char *actname)
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
static struct mon_action_args* build_action_args(char **argv, size_t *idx)
{
	size_t i, cnt;
	struct mon_action_args *args =
		calloc(1, sizeof(struct mon_action_args));
	if (!args)
		goto error;
	cnt = 0;
	while (1) {
		const char *t = argv[*idx + cnt];
		if (t == NULL)
			break;
		if (index(t, '=') == NULL)
			break;
		++cnt;
	}
	args->name = calloc(cnt + 1, sizeof(const char*));
	if (!args->name)
		goto error;
	args->val = calloc(cnt + 1, sizeof(const char*));
	if (!args->val)
		goto error;
	for (i = 0; i < cnt; ++i) {
		char *eq;
		args->name[i] = strdup(argv[*idx + i]);
		if (!args->name[i])
			goto error;
		eq = index(args->name[i], '=');
		*eq = '\0';
		args->val[i] = strdup(eq + 1);
	}
	*idx = *idx + cnt;
	return args;
error:
	free_mon_action_args(args);
	return NULL;
}

static void validate_action_args(char *error, size_t error_len, 
	const struct mon_action *act, struct mon_action_args *args)
{
	char **n;
	for (n = args->name; *n; ++n) {
		const char **a;
		for (a = act->args; *a; ++a) {
			if (strcmp(*a, *n) == 0) {
				break;
			}
		}
		if (*a == NULL) {
			snprintf(error, error_len, "Parse error: action "
				 "'%s' does not take '%s' as an argument.\n",
				 act->names[0], *n);
			return;
		}
	}
}

void parse_mon_actions(char ** argv, char *error, size_t error_len,
		       const struct mon_action ***mon_actions,
		       struct mon_action_args ***mon_args)
{
	size_t i, idx, num_acts = 0;

	memset(error, 0, error_len);
	*mon_actions = NULL;
	*mon_args = NULL;

	*mon_actions = calloc(1, sizeof(void*));
	if (!*mon_actions)
		goto malloc_error;
	*mon_args = calloc(1, sizeof(void*));
	if (!*mon_args)
		goto malloc_error;
	for (idx = 0; argv[idx]; ) {
		struct mon_action_args **argl, *arg;
		const struct mon_action **acts;
		const struct mon_action *act = parse_one_action(argv[idx]);
		if (!act) {
			snprintf(error, error_len, "There is no monitor action "
				 "named '%s'. Please enter a valid action.\n",
				 argv[idx]);
			goto error;
		}
		acts = realloc(*mon_actions,
			    sizeof(struct mon_action*) * (num_acts + 2));
		if (!acts)
			goto malloc_error;
		*mon_actions = acts;
		acts[num_acts] = act;
		acts[num_acts + 1] = NULL;
		++idx;
		arg = build_action_args(argv, &idx);
		if (!arg)
			goto malloc_error;
		argl = realloc(*mon_args,
			    sizeof(struct mon_action_args*) * (num_acts + 2));
		if (!argl)
			goto malloc_error;
		*mon_args = argl;
		argl[num_acts] = arg;
		argl[num_acts + 1] = NULL;
		++num_acts;
		validate_action_args(error, error_len, act, arg);
		if (error[0])
			goto error;
	}
	if (num_acts == 0) {
		snprintf(error, error_len, "You must give at least one "
			 "action for fishmon to execute.");
		goto error;
	}
	return;

malloc_error:
	snprintf(error, error_len, "malloc failed.");
error:
	free(*mon_actions);
	*mon_actions = NULL;
	if (*mon_args) {
		for (i = 0; i < num_acts; ++i) {
			free_mon_action_args(*mon_args[i]);
		}
		free(*mon_args);
		*mon_args = NULL;
	}
}
