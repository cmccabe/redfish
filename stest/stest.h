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

#ifndef REDFISH_STEST_STEST_DOT_H
#define REDFISH_STEST_STEST_DOT_H

#include "util/compiler.h"

struct redfish_client;
struct redfish_dir_entry;
struct redfish_mds_locator;

/** Get the Redfish configuration file and user name from the environment.
 *
 * Die if the configuration file has not been specified.
 *
 * @param conf		(out param) statically allocated string with the conf
 *			file path
 * @param user		(out param) statically allocated string with the user
 *			name
 */
extern void stest_get_conf_and_user(const char **conf, const char **user);

/** Parse argv.
 *
 * Custom options are given on the commandline as key=val.
 *
 * Will abort the program if a parse error is encountered.
 *
 * @param argc		argc as passed to main
 * @param argv		argv as passed to main
 * @param usage		NULL-terminated list of test-specific usage lines.
 * 			These will be printed if the user specifies '-h'
 * 			of if there is an option parsing error.
 */
extern void stest_init(int argc, char **argv, const char **test_usage);

/** Set the stest done status
 *
 * @param pdone		Percent done, from 0 to 100
 */
extern void stest_set_status(int pdone);

/** Log an error that happened during our test.
 *
 * @param err		The format string
 * @param ...		Printf-style arguments
 */
extern void stest_add_error(const char *fmt, ...) PRINTF_FORMAT(1, 2);

/** Should be called at the end of a system test main().
 *
 * @return		The status that should be returned from the main
 *			function of a system test.
 */
extern int stest_finish(void);

/** result from stest_stat */
enum stest_stat_res {
	STEST_STAT_RES_ERR = 1,
	STEST_STAT_RES_ENOENT = 2,
	STEST_STAT_RES_FILE = 100,
	STEST_STAT_RES_DIR = 101,
};

/** Simple wrapper function for redfish_stat
 *
 * @param cli		Redfish client
 * @param path		path to stat
 *
 * @return		the result
 */
extern enum stest_stat_res stest_stat(struct redfish_client *cli,
				      const char *path);

/** Function that examines directory entries for stest_listdir */
typedef int (*stest_listdir_fn)(const struct redfish_dir_entry *oda, void *v);

/** Simple wrapper function to test redfish_listdir
 *
 * @param cli		Redfish client
 * @param path		listdir path
 * @param fn		function to invoke on each entry
 * @param data		data to provide to fn
 *
 * @return		a negative number on listdir error; the number of
 *			entries processed otherwise.
 */
int stest_listdir(struct redfish_client *cli, const char *path,
		stest_listdir_fn fn, void *data);

#define ST_EXPECT_ZERO(expr) \
	do { \
		int __e__ = expr; \
		if (__e__) { \
			stest_add_error("failed on file %s, line %d: %s\n" \
					"expected 0, got %d\n", \
					__FILE__, __LINE__, #expr, __e__); \
			return 1; \
		} \
	} while(0);

#define ST_EXPECT_NONZERO(expr) \
	do { \
		if (!expr) { \
			stest_add_error("failed on file %s, line %d: %s\n", \
					"expected nonzero, got 0\n", \
					__FILE__, __LINE__, #expr); \
			return 1; \
		} \
	} while(0);

#define ST_EXPECT_EQ(expr1,  expr2) \
	do { \
		if ((expr1) != (expr2)) { \
			stest_add_error("failed on file %s, line %d\n", \
					__FILE__, __LINE__); \
			return 1; \
		} \
	} while(0);

#define ST_EXPECT_NOT_EQ(expr1, expr2) \
	do { \
		if ((expr1) == (expr2)) { \
			stest_add_error("failed on file %s, line %d\n", \
					__FILE__, __LINE__); \
			return 1; \
		} \
	} while(0);

#define STEST_REDFISH_CONF_EXPLANATION \
	"REDFISH_CONF: sets the path to the Redfish configuration file"
#define STEST_REDFISH_USER_EXPLANATION \
	"REDFISH_USER: sets the Redfish user to use.  Defaults to superuser."

#endif
