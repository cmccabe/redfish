/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "top/fscreen.h"
#include "top/state.h"
#include "top/gfx.h"
#include "util/error.h"

#include <curses.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_TEST_LINES 300

struct fscreen* gfx_fscreen_create(const struct top_state *ts)
{
	struct fscreen *sn = fscreen_init();
	if (!sn) {
		return NULL;
	}
	sn->lines = NULL;
	sn->num_lines = 0;
	sn->scroll_pos = 0;

	sn->header = fscreen_makeline(COLOR_PAIR(4) | A_REVERSE,
		"OneFish", COLOR_PAIR(4) | A_REVERSE,
		ts->conn_status, COLOR_PAIR(4) | A_REVERSE,
		(char*)NULL);
	if (!sn->header) {
		fscreen_free(sn);
		return NULL;
	}
	sn->footer = fscreen_makeline(COLOR_PAIR(4) | A_REVERSE,
		"[d]aemons", COLOR_PAIR(4) | A_REVERSE,
		"[a]ctions", COLOR_PAIR(4) | A_REVERSE,
		"[w]arnings", COLOR_PAIR(4) | A_REVERSE,
		(char*)NULL);
	if (!sn->footer) {
		fscreen_free(sn);
		return NULL;
	}
	return sn;
}

void gfx_set_title_bar_to_disconnected(struct fscreen *sn, char *err)
{
	char *t, tbar[512];
	snprintf(tbar, sizeof(tbar), "OneFish [disconnected: %s]", err);
	t = strdup(tbar);
	if (t) {
		free(sn->header->segs->text);
		sn->header->segs->text = t;
	}
}

int gfx_set_test_lines(struct fscreen *sn)
{
	int i;

	/* set up test lines */
	fscreen_wipelines(sn);
	sn->lines = calloc(1, sizeof(struct fscreen_line*) * NUM_TEST_LINES);
	if (!sn->lines)
		return -ENOMEM;
	for (i = 0; i < NUM_TEST_LINES; ++i) {
		char buf[128];
		snprintf(buf, sizeof(buf), "line number %d", i + 1);
		sn->lines[i] = fscreen_makeline(COLOR_PAIR(7), buf,
						COLOR_PAIR(7), (char*)NULL);
		if (!sn->lines[i]) {
			fscreen_wipelines(sn);
			return -ENOMEM;
		}
	}
	sn->num_lines = NUM_TEST_LINES;
	return 0;
}

int gfx_top_actions_to_fscreen(struct top_action **tacts, struct fscreen *sn)
{
	int nl = 1;
	struct top_action **t;

	fscreen_wipelines(sn);
	for (t = tacts; *t; ++t) {
		++nl;
	}
	sn->lines = calloc(1, sizeof(struct fscreen_line*) * nl);
	if (!sn->lines)
		return -ENOMEM;
	sn->lines[0] = fscreen_makeline(COLOR_PAIR(7), "ACTION", COLOR_PAIR(7),
						"COMPLETE", COLOR_PAIR(7),
						(char*)NULL);
	if (!sn->lines[0]) {
		fscreen_wipelines(sn);
		return -ENOMEM;
	}
	for (t = tacts; *t; ++t) {
		char complete[16];
		snprintf(complete, sizeof(complete), "%d%%", (*t)->complete);
		sn->lines[nl] = fscreen_makeline(COLOR_PAIR(7), (*t)->name,
						COLOR_PAIR(7), complete,
						COLOR_PAIR(7), (char*)NULL);
		if (!sn->lines[nl]) {
			fscreen_wipelines(sn);
			return -ENOMEM;
		}
		++nl;
	}
	sn->num_lines = nl;
	return 0;
}
