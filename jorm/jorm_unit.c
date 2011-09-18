/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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

int test1(void)
{
	int ret;
	const char *str;
	struct json_object *jo = NULL;
	struct abbie *my_abbie = calloc(1, sizeof(struct abbie));
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

int test2(void)
{
	int i, ret;
	const char *str;
	struct json_object *jo = NULL;
	struct abbie *my_abbie[TEST2_NUM_ABBIE + 1] = { 0 };
	struct bob *my_bob;
	for (i = 0; i < TEST2_NUM_ABBIE; ++i) {
		my_abbie[i] = calloc(1, sizeof(struct abbie));
		if (!my_abbie[i]) {
			ret = EXIT_FAILURE;
			goto done;
		}
		my_abbie[i]->a = i;
	}
	my_bob = calloc(1, sizeof(struct bob));
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
	jo = JORM_TOJSON_bob(my_bob);
	if (!jo) {
		ret = EXIT_FAILURE;
		goto done;
	}
	str = json_object_to_json_string(jo);
	if (strcmp(str, "{ \"a\": 1, \"b\": 2.500000, \"c\": \"hi there\", "
		   "\"d\": { \"a\": 0 }, \"e\": false, \"f\": [ { \"a\": 1 }, "
		   "{ \"a\": 2 } ] }") != 0)
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
		free(my_bob);
	}
	for (i = 0; i < TEST2_NUM_ABBIE; ++i) {
		free(my_abbie[i]);
		my_abbie[i] = NULL;
	}
	return ret;
}

int test3(void)
{
	int ret;
	char err[512] = { 0 };
	const char in_str[] = "{ \"a\": 1, \"b\": 2.500000, "
		"\"c\": \"hi there\", \"d\": { \"a\": 0 }, "
		"\"e\": false, \"f\": [ { \"a\": 1 }, "
		"{ \"a\": 2 } ] }";
	struct json_object* jo = NULL;
	struct bob *my_bob = NULL;

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

int main(void)
{
	EXPECT_ZERO(test1());
	EXPECT_ZERO(test2());
	EXPECT_ZERO(test3());
	return EXIT_SUCCESS;
}
