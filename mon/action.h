/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_MON_ACTION_DOT_H
#define ONEFISH_MON_ACTION_DOT_H

#include <stdio.h> /* for FILE* */
#include <unistd.h> /* for size_t */

struct mon_action_args {
	char **name;
	char **val;
};

/** Extract an argument from a mon_action_args structure.
 *
 * @param args			The args
 * @param name			The name of the argument to grab
 * @param default_val		The value to return if the argument wasn't 
 *				given
 *				
 * @returns			a pointer to the argument value
 */
const char *get_mon_action_arg(const struct mon_action_args *args,
			const char *name, const char *default_val);

/** Release the memory associated with some monitor action arguments
 *
 * @param args			The args
 */
void free_mon_action_args(struct mon_action_args* args);

enum mon_action_ty {
	MON_ACTION_ADMIN,
	MON_ACTION_TEST,
	MON_ACTION_UNIT_TEST,
};

typedef int (*action_fn_t)(const struct mon_action_args *args);

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

/** Print a series of lines
 *
 * @param lines			NULL-terminated array of lines to print.
 */
void print_lines(FILE *fp, const char **lines);

/** Parse some monitor actions passed in on argv
 *
 * @param argv		A NULL-terminated array of strings.
 * @param error		A buffer where we can write an error string if need be.
 * @param error_len	Length of the error buffer.
 * @param mon_actions	Where to write the (dynamically allocated)
 * 			NULL-terminated array of pointers to monitor actions.
 * @param mon_args	Where to write the (dynamically allocated)
 * 			NULL-terminated array of pointers to monitor arguments.
 */
void parse_mon_actions(char **argv, char *error, size_t error_len,
		       const struct mon_action ***mon_actions,
		       struct mon_action_args ***mon_args);

#endif
