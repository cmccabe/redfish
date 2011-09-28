/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_STEST_STEST_DOT_H
#define ONEFISH_STEST_STEST_DOT_H

struct of_mds_locator;

struct stest_custom_opt
{
	/** Pointer to a key which identifies the option */
	const char *key;

	/** Pointer to a statically allocated string */
	char *val;

	/** Help string to print out in usage function */
	const char *help;
};

/** Get a custom option value from a list of custom options
 *
 * @param key		the key to get
 * @param copt		array of custom options
 * @param ncopt		length of copt
 *
 * @returnv		NULL if key isn't found; the matchinvalue otherwise
 */
const char *copt_get(const char *key, struct stest_custom_opt *copt,
		     int ncopt);

/** Parse argv.
 *
 * Custom options are given on the commandline as key=val.
 *
 * Will abort the program if a parse error is encountered. 
 *
 * @param argc		argc as passed to main
 * @param argv		argv as passed to main
 * @param copt		(in-out param) NULL-terminated list of custom stest
 *			options. All custom options are treated as
 *			non-mandatory. Custom options not found on the
 *			commandline will not have their entries in copt
 *			altered.
 * @param user		(out-param) The username to connect as. Statically
 *			allocated.
 * @param mlocs		(out-param) an array of mds locators. Dynamically
 *			allocated.
 * @param ncopt		number of custom options
 */
extern void stest_init(int argc, char **argv, struct stest_custom_opt *copt,
		       int ncopt, const char **user, struct of_mds_locator ***mlocs);

/** Free mlocs array returned from stest_init
 *
 * @param mlocs		the mlocs array returned from stest_init
 */
void stest_mlocs_free(struct of_mds_locator **mlocs);

/** Set the stest done status
 *
 * @param pdone		Percent done, from 0 to 100
 */
extern void stest_set_status(int pdone);

/** Log an error that happened during our test.
 *
 * @param err		The error
 */
extern void stest_add_error(const char *err);

/** Should be called at the end of a system test main().
 *
 * @return		The status that should be returned from the main
 *			function of a system test.
 */
extern int stest_finish(void);

#endif
