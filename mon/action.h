/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_MON_ACTION_DOT_H
#define ONEFISH_MON_ACTION_DOT_H

#include <unistd.h> /* for size_t */

struct action_arg;
struct action_info;
struct mon_info;

/** Extract an argument from a mon_action_args structure.
 *
 * @param args			The action arguments
 * @param name			The name of the argument to grab
 * @param default_val		The value to return if the argument wasn't 
 *				given
 *				
 * @returns			a pointer to the argument value
 */
const char *get_mon_action_arg(struct action_arg **args,
			const char *name, const char *default_val);

enum mon_action_ty {
	MON_ACTION_ADMIN,
	MON_ACTION_IDLE,
	MON_ACTION_TEST,
	MON_ACTION_UNIT_TEST,
};

typedef int (*action_fn_t)(struct action_info *ai, struct action_arg **args);

/** Describes a monitor action that the user can request.
 *
 * Actions can be administrative, like installing new software on the cluster,
 * or test-related, like running round_trip_one_file_test.
 *
 */
struct mon_action {
	/** The type of action */
	enum mon_action_ty ty;

	/** A NULL-terminated array of names for this action. */
	const char **names;

	/** A NULL-terminated array of description text lines. */
	const char **desc;

	/** A NULL-terminated array of valid argument names. */
	const char **args;

	/** The function to run when the action is executed */
	action_fn_t fn;
};

/** Print a description of all actions with type ty
 *
 * @param ty			The type of action to print information about
 */
void print_action_descriptions(enum mon_action_ty ty);

/** Given an action name, return a pointer to the action structure
 *
 * @param actname		The action name
 */
const struct mon_action* parse_one_action(const char *actname);

/** Parse some monitor actions passed in on argv
 *
 * @param argv		A NULL-terminated array of strings.
 * @param err		A buffer where we can write an error string if need be.
 * @param err_len	Length of the error buffer.
 *
 * @return		A monitor info structure, or NULL on error.
 */
struct action_info** argv_to_action_info(char **argv, char *err, size_t err_len);

#endif
