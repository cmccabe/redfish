/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "top/fscreen.h"
#include "top/gfx.h"
#include "top/state.h"

#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int init_top_state(struct top_state* ts, int test)
{
	struct top_action **tacts;
	memset(ts, 0, sizeof(struct top_state));
	tacts = calloc(1, sizeof(struct top_action*));
	if (!tacts)
		return -ENOMEM;
	ts->view = (test) ? CUR_VIEW_TEST_LINES : CUR_VIEW_DAEMONS;
	ts->tacts = tacts;
	if (test) {
		snprintf(ts->conn_status, TS_CONN_STATUS_LEN, "Test");
	}
	else {
		snprintf(ts->conn_status, TS_CONN_STATUS_LEN, "Connected");
	}
	ts->scroll_pos = 0;
	ts->need_redraw = 0;
	ts->done = 0;
	return 0;
}

void clear_top_state(struct top_state *ts)
{
	free(ts->tacts);
	ts->tacts = NULL;
}

struct fscreen *write_fscreen(const struct top_state *ts)
{
	struct fscreen *sn = gfx_fscreen_create(ts);
	if (!sn)
		return NULL;
	switch (ts->view) {
	case CUR_VIEW_TEST_LINES:
		if (gfx_set_test_lines(sn))
			goto error;
		break;
	case CUR_VIEW_DAEMONS:
		if (gfx_set_test_lines(sn))
			goto error;
		break;
	case CUR_VIEW_ACTIONS:
		if (gfx_set_test_lines(sn))
			goto error;
		break;
	case CUR_VIEW_WARNINGS:
		if (gfx_set_test_lines(sn))
			goto error;
		break;
	default:
		/* logic error */
		abort();
		break;
	}
	return sn;
error:
	fscreen_free(sn);
	return NULL;
}

static void top_state_set_view(struct top_state *ts, enum cur_view view)
{
	if (ts->view == view)
		return;
	ts->view = view;
	ts->need_redraw = 1;
}

void handle_keyboard_input(struct top_state *ts)
{
	int mx, my;
	chtype k = getch();
	switch (k) {
	case 'a':
		top_state_set_view(ts, CUR_VIEW_ACTIONS);
		break;
	case 'd':
		top_state_set_view(ts, CUR_VIEW_DAEMONS);
		break;
	case 'w':
		top_state_set_view(ts, CUR_VIEW_WARNINGS);
		break;
	case KEY_UP:
	case 'k':
		ts->scroll_pos--;
		ts->need_redraw = 1;
		break;
	case KEY_DOWN:
	case 'j':
		ts->scroll_pos++;
		ts->need_redraw = 1;
		break;
	case KEY_PPAGE:
		getmaxyx(stdscr, my, mx);
		ts->scroll_pos -= my / 2;
		ts->need_redraw = 1;
		break;
	case KEY_NPAGE:
		getmaxyx(stdscr, my, mx);
		ts->scroll_pos += my / 2;
		ts->need_redraw = 1;
		break;
	case 'Q':
	case 'q':
		ts->done = 1;
		break;
	default:
		/* ignore */
		break;
	}
}
