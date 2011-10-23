/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */


#include "util/queue.h"
#include "util/test.h"
#include "util/tree.h"

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define NUM_FOO 100

struct foo {
	int x;
	int y;
	RB_ENTRY(foo) tree_entry;
	STAILQ_ENTRY(foo) queue_entry;
};

static int foo_compare(struct foo *a, struct foo *b)
{
	if (a->x < b->x)
		return -1;
	if (a->x > b->x)
		return 1;
	if (a->y < b->y)
		return -1;
	if (a->y > b->y)
		return 1;
	return 0;
}

STAILQ_HEAD(test_queue, foo);

static int test_queue(void)
{
	int i, num_elem;
	struct test_queue head;
	struct foo *f;
	STAILQ_INIT(&head);

	for (i = 0; i < NUM_FOO; ++i) {
		f = calloc(1, sizeof(struct foo));
		if (!f)
			return -ENOMEM;
		f->x = (i * 283) % 17;
		f->y = (i * 499) % 19;
		STAILQ_INSERT_TAIL(&head, f, queue_entry);
	}

	f = STAILQ_LAST(&head, foo, queue_entry);
	EXPECT_EQUAL(f->x, 1);
	EXPECT_EQUAL(f->y, 1);

	num_elem = 0; 
	while (!STAILQ_EMPTY(&head)) {
		f = STAILQ_FIRST(&head);
		++num_elem;
		STAILQ_REMOVE_HEAD(&head, queue_entry);
		free(f);
	}

	EXPECT_EQUAL(num_elem, NUM_FOO);

	return 0;
}

RB_HEAD(test_tree, foo);
RB_GENERATE(test_tree, foo, tree_entry, foo_compare);

static int test_tree(void)
{
	int i, num_elem;
	struct test_tree head;
	struct foo *f, *f_tmp;
	RB_INIT(&head);

	for (i = 0; i < NUM_FOO; ++i) {
		f = calloc(1, sizeof(struct foo));
		if (!f)
			return -ENOMEM;
		f->x = (i * 283) % 17;
		f->y = (i * 499) % 19;

		RB_INSERT(test_tree, &head, f);
	}

	f = RB_MAX(test_tree, &head);
	EXPECT_EQUAL(f->x, 16);
	EXPECT_EQUAL(f->y, 15);

	f = RB_MIN(test_tree, &head);
	EXPECT_EQUAL(f->x, 0);
	EXPECT_EQUAL(f->y, 0);

	num_elem = 0;
	RB_FOREACH_SAFE(f, test_tree, &head, f_tmp) {
		RB_REMOVE(test_tree, &head, f);
		free(f);
		++num_elem;
	}

	EXPECT_EQUAL(num_elem, NUM_FOO);
	return 0;
}

int main(void)
{
	EXPECT_ZERO(test_queue());
	EXPECT_ZERO(test_tree());

	return EXIT_SUCCESS;
}
