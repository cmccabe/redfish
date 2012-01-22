/*
 * Copyright 2011-2012 the RedFish authors
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
	EXPECT_EQ(f->x, 1);
	EXPECT_EQ(f->y, 1);

	num_elem = 0;
	while (!STAILQ_EMPTY(&head)) {
		f = STAILQ_FIRST(&head);
		++num_elem;
		STAILQ_REMOVE_HEAD(&head, queue_entry);
		free(f);
	}

	EXPECT_EQ(num_elem, NUM_FOO);

	return 0;
}

RB_HEAD(test_tree, foo);
RB_GENERATE(test_tree, foo, tree_entry, foo_compare);

static int test_tree(void)
{
	int i, num_elem;
	struct test_tree head;
	struct foo *f, *f_tmp, f2, *f3, *f4;
	RB_INIT(&head);

	for (i = 0; i < NUM_FOO; ++i) {
		f = calloc(1, sizeof(struct foo));
		if (!f)
			return -ENOMEM;
		f->x = (i * 283) % 17;
		f->y = (i * 499) % 19;

		RB_INSERT(test_tree, &head, f);
	}

	/* test max */
	f = RB_MAX(test_tree, &head);
	EXPECT_EQ(f->x, 16);
	EXPECT_EQ(f->y, 15);

	/* (try to) insert duplicate */
	f2.x = 16;
	f2.y = 15;
	f3 = RB_INSERT(test_tree, &head, &f2);
	EXPECT_EQ(f, f3);
	f4 = RB_MAX(test_tree, &head);
	EXPECT_EQ(f, f4);

	/* test min */
	f = RB_MIN(test_tree, &head);
	EXPECT_EQ(f->x, 0);
	EXPECT_EQ(f->y, 0);

	num_elem = 0;
	RB_FOREACH_SAFE(f, test_tree, &head, f_tmp) {
		RB_REMOVE(test_tree, &head, f);
		free(f);
		++num_elem;
	}

	EXPECT_EQ(num_elem, NUM_FOO);
	return 0;
}

int main(void)
{
	EXPECT_ZERO(test_queue());
	EXPECT_ZERO(test_tree());

	return EXIT_SUCCESS;
}
