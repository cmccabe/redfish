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

#include "jorm/jorm_unit.h"
#include "util/test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JORM_CUR_FILE "jorm/jorm_unit.jorm"
#include "jorm/jorm_generate_body.h"
#undef JORM_CUR_FILE

#define TEST2_NUM_ABBIE 3

static int test_jorm_init(void)
{
	struct bob *b1 = JORM_INIT_bob();
	EXPECT_NOT_EQ(b1, NULL);
	EXPECT_EQ(b1->a, JORM_INVAL_INT);
	EXPECT_EQ(b1->b, JORM_INVAL_DOUBLE);
	EXPECT_EQ(b1->c, JORM_INVAL_STR);
	EXPECT_EQ(b1->d, JORM_INVAL_NESTED);
	EXPECT_EQ(b1->e, JORM_INVAL_BOOL);
	JORM_FREE_bob(b1);
	return 0;
}

static int test1(void)
{
	int ret;
	const char *str;
	struct json_object *jo = NULL;
	struct abbie *my_abbie = JORM_INIT_abbie();
	my_abbie->a = 456;
	jo = JORM_TOJSON_abbie(my_abbie);
	if (!jo) {
		ret = EXIT_FAILURE;
		goto done;
	}
	str = json_object_to_json_string(jo);
	if (strcmp(str, "{ \"a\": 456 }") != 0) {
		ret = EXIT_FAILURE;
		goto done;
	}
	ret = 0;
done:
	if (jo) {
		json_object_put(jo);
		jo = NULL;
	}
	free(my_abbie);
	return ret;
}

static int test2(void)
{
	int i, ret;
	const char *str;
	struct json_object *jo = NULL;
	struct abbie *my_abbie[TEST2_NUM_ABBIE + 1] = { 0 };
	struct bob *my_bob = NULL;
	for (i = 0; i < TEST2_NUM_ABBIE; ++i) {
		my_abbie[i] = JORM_INIT_abbie();
		if (!my_abbie[i]) {
			ret = EXIT_FAILURE;
			goto done;
		}
		my_abbie[i]->a = i;
	}
	my_bob = JORM_INIT_bob();
	if (!my_bob) {
		ret = EXIT_FAILURE;
		goto done;
	}
	my_bob->a = 1;
	my_bob->b = 2.5;
	my_bob->c = strdup("hi there");
	if (!my_bob->c) {
		ret = EXIT_FAILURE;
		goto done;
	}
	my_bob->d = my_abbie[0];
	my_bob->e = 0;
	my_bob->f = calloc(3, sizeof(struct abbie*));
	my_bob->f[0] = my_abbie[1];
	my_bob->f[1] = my_abbie[2];
	my_bob->f[2] = NULL;
	my_bob->extra_data = 404;
	my_bob->g = JORM_INIT_carrie();
	EXPECT_NOT_EQ(my_bob->g, NULL);
	my_bob->g->x = 101;
	my_bob->g->y = 5.0;
	jo = JORM_TOJSON_bob(my_bob);
	if (!jo) {
		ret = EXIT_FAILURE;
		goto done;
	}
	str = json_object_to_json_string(jo);
	if (strcmp(str, "{ \"a\": 1, \"b\": 2.500000, \"c\": \"hi there\", "
		   "\"d\": { \"a\": 0 }, \"e\": false, \"f\": [ { \"a\": 1 }, "
		   "{ \"a\": 2 } ], \"x\": 101, \"y\": 5.000000 }") != 0)
	{
		fprintf(stderr, "got str = '%s'\n", str);
		ret = EXIT_FAILURE;
		goto done;
	}
	ret = 0;
done:
	if (jo) {
		json_object_put(jo);
		jo = NULL;
	}
	if (my_bob) {
		if (my_bob->c) {
			free(my_bob->c);
			my_bob->c = NULL;
		}
		if (my_bob->f) {
			free(my_bob->f);
			my_bob->f = NULL;
		}
		if (my_bob->g) {
			free(my_bob->g);
			my_bob->g = NULL;
		}
		free(my_bob);
	}
	for (i = 0; i < TEST2_NUM_ABBIE; ++i) {
		free(my_abbie[i]);
		my_abbie[i] = NULL;
	}
	return ret;
}

static int test3(void)
{
	size_t i;
	int ret;
	char err[512] = { 0 };
	const char in_str[] = "{ \"a\": 1, \"b\": 2.500000, "
		"\"c\": \"hi there\", \"d\": { \"a\": 0 }, "
		"\"e\": false, \"f\": [ { \"a\": 1 }, "
		"{ \"a\": 2 } ], \"x\" : 5, \"y\" : 1.5 }";
	int expected_array_val[] = { 1, 2, 6 };
	struct json_object* jo = NULL;
	struct bob *my_bob = NULL;
	struct abbie* final_abbie;

	jo = parse_json_string(in_str, err, sizeof(err));
	if (err[0]) {
		fprintf(stderr, "parse_json_string error: %s\n", err);
		ret = EXIT_FAILURE;
		goto done;
	}
	my_bob = JORM_FROMJSON_bob(jo);
	if (!my_bob) {
		fprintf(stderr, "JORM_FROMJSON: OOM\n");
		ret = EXIT_FAILURE;
		goto done;
	}
	ret = 0;
	EXPECT_NONZERO(my_bob->a == 1);
	EXPECT_NONZERO(my_bob->b == 2.5);
	EXPECT_ZERO(my_bob->extra_data);
	final_abbie = JORM_OARRAY_APPEND_abbie(&my_bob->f);
	EXPECT_NOT_EQ(final_abbie, NULL);
	final_abbie->a = 6;
	for (i = 0; i < sizeof(expected_array_val) /
			sizeof(expected_array_val[0]); ++i) {
		EXPECT_EQ(my_bob->f[i]->a, expected_array_val[i]);
	}
	EXPECT_EQ(my_bob->g->x, 5);
	EXPECT_EQ(my_bob->g->y, 1.5);
done:
	if (jo) {
		json_object_put(jo);
		jo = NULL;
	}
	if (my_bob) {
		JORM_FREE_bob(my_bob);
		my_bob = NULL;
	}
	return ret;
}

static int test4(void)
{
	struct bob *b1 = JORM_INIT_bob();
	struct bob *b2 = JORM_INIT_bob();
	struct bob *b3 = JORM_INIT_bob();
	struct abbie **abbie_arr = NULL;

	EXPECT_NOT_EQ(b1, NULL);
	EXPECT_NOT_EQ(b2, NULL);
	EXPECT_NOT_EQ(b3, NULL);

	EXPECT_EQ(b3->a, JORM_INVAL_INT);
	b1->a = 101;
	EXPECT_ZERO(JORM_COPY_bob(b1, b3));
	EXPECT_EQ(b3->a, 101);

	b2->a = JORM_INVAL_INT;
	b2->b = 50;
	b2->d = JORM_INIT_abbie();
	b2->d->a = 9000;
	EXPECT_ZERO(JORM_COPY_bob(b2, b3));
	EXPECT_EQ(b3->b, 50);
	EXPECT_NOT_EQ(b3->d, NULL);
	EXPECT_EQ(b3->d->a, 9000);
	EXPECT_EQ(b3->a, 101);

	b1->f = calloc(3, sizeof(struct abbie*));
	EXPECT_NOT_EQ(b1->f, NULL);
	b1->f[0] = JORM_INIT_abbie();
	EXPECT_NOT_EQ(b1->f[0], NULL);
	b1->f[1] = JORM_INIT_abbie();
	EXPECT_NOT_EQ(b1->f[1], NULL);
	b1->f[2] = NULL;
	b1->f[0]->a = 100;
	b1->f[1]->a = 200;
	EXPECT_ZERO(JORM_COPY_bob(b1, b3));
	EXPECT_NOT_EQ(b3->f, NULL);
	EXPECT_NOT_EQ(b3->f[0], NULL);
	EXPECT_EQ(b3->f[0]->a, 100);
	EXPECT_NOT_EQ(b3->f[1], NULL);
	EXPECT_EQ(b3->f[1]->a, 200);
	EXPECT_EQ(b3->f[2], NULL);

	abbie_arr = JORM_OARRAY_COPY_abbie(b3->f);
	EXPECT_NOT_EQ(abbie_arr, NULL);
	EXPECT_NOT_EQ(abbie_arr[0], NULL);
	EXPECT_EQ(abbie_arr[0]->a, 100);
	EXPECT_NOT_EQ(abbie_arr[1], NULL);
	EXPECT_EQ(abbie_arr[1]->a, 200);
	EXPECT_EQ(abbie_arr[2], NULL);

	JORM_OARRAY_FREE_abbie(&abbie_arr);
	JORM_FREE_bob(b1);
	JORM_FREE_bob(b2);
	JORM_FREE_bob(b3);
	return 0;
}

static int test5(void)
{
	int ret;
	char acc[512] = { 0 }, err[512] = { 0 };
	struct json_object *jo = NULL;
	const char in_str[] = "{ \"a\": \"1\", \"b\": 2.500000, "
		"\"c\": 5.0, \"d\": { \"a\": false }, "
		"\"e\": false, \"f\": [ { \"a\": 1 }, "
		"{ \"a\": 2 } ], \"x\" : 5, \"y\" : 1 }";
	const char in_str2[] = "{ \"d\": false, \"f\" : 1 }";
	const char in_str3[] = "{ \"f\" : [ { \"a\" : 2.5 }, 1 ] }";
	jo = parse_json_string(in_str, err, sizeof(err));
	if (err[0]) {
		fprintf(stderr, "parse_json_string error: %s\n", err);
		ret = EXIT_FAILURE;
		goto done;
	}
	JORM_TYCHECK_bob(jo, acc, sizeof(acc), err, sizeof(err));
	EXPECT_ZERO(strcmp(err, "WARNING: ignoring field \"a\" because "
		"it has type string, but it should have type int.\n"
		"WARNING: ignoring field \"c\" because it has type double, "
		"but it should have type string.\n"
		"WARNING: ignoring field \"d/a\" because it has type boolean, "
		"but it should have type int.\n"
		"WARNING: ignoring field \"y\" because it has type int, "
		"but it should have type double.\n"));

	acc[0] = '\0';
	err[0] = '\0';
	json_object_put(jo);
	jo = parse_json_string(in_str2, err, sizeof(err));
	if (err[0]) {
		fprintf(stderr, "parse_json_string2 error: %s\n", err);
		ret = EXIT_FAILURE;
		goto done;
	}
	JORM_TYCHECK_bob(jo, acc, sizeof(acc), err, sizeof(err));
	EXPECT_ZERO(strcmp(err, "WARNING: ignoring field \"d\" because "
		"it has type boolean, but it should have type object.\n"
		"WARNING: ignoring field \"f\" because it has type "
		"int, but it should have type array.\n"));

	acc[0] = '\0';
	err[0] = '\0';
	json_object_put(jo);
	jo = parse_json_string(in_str3, err, sizeof(err));
	if (err[0]) {
		fprintf(stderr, "parse_json_string3 error: %s\n", err);
		ret = EXIT_FAILURE;
		goto done;
	}
	JORM_TYCHECK_bob(jo, acc, sizeof(acc), err, sizeof(err));
	EXPECT_ZERO(strcmp(err, "WARNING: ignoring field \"f[0]/a\" because "
		"it has type double, but it should have type int.\n"
		"WARNING: ignoring field \"f[1]\" because "
		"it has type array, but it should have type int.\n"));

	ret = 0;
done:
	if (jo)
		json_object_put(jo);
	return ret;
}

static int test_jorm_array_manipulations(void)
{
	struct bob **a1 = NULL, **a2 = NULL;
	struct bob *b, *b2, *b3;

	b = JORM_OARRAY_APPEND_bob(&a1);
	EXPECT_NOT_EQ(b, NULL);
	EXPECT_EQ(b->a, JORM_INVAL_INT);
	EXPECT_NOT_EQ(a1[0], NULL);
	JORM_OARRAY_REMOVE_bob(&a1, b);
	EXPECT_EQ(a1[0], NULL);
	b = JORM_OARRAY_APPEND_bob(&a2);
	b->a = 1;
	b2 = JORM_OARRAY_APPEND_bob(&a2);
	b2->a = 2;
	b3 = JORM_OARRAY_APPEND_bob(&a2);
	b3->a = 3;
	JORM_OARRAY_REMOVE_bob(&a2, b2);
	EXPECT_EQ(a2[0], b);
	EXPECT_EQ(a2[1], b3);
	EXPECT_EQ(a2[2], NULL);
	JORM_OARRAY_FREE_bob(&a1);
	JORM_OARRAY_FREE_bob(&a2);
	return 0;
}

int main(void)
{
	EXPECT_ZERO(test_jorm_init());
	EXPECT_ZERO(test1());
	EXPECT_ZERO(test2());
	EXPECT_ZERO(test3());
	EXPECT_ZERO(test4());
	EXPECT_ZERO(test5());
	EXPECT_ZERO(test_jorm_array_manipulations());
	return EXIT_SUCCESS;
}
