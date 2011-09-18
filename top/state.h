/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_TOP_STATE_DOT_H
#define ONEFISH_TOP_STATE_DOT_H

#define TS_CONN_STATUS_LEN 64

struct fscreen;

enum cur_view {
	CUR_VIEW_TEST_LINES = 0,
	CUR_VIEW_DAEMONS,
	CUR_VIEW_ACTIONS,
	CUR_VIEW_WARNINGS,
};

struct top_action {
	const char *name;
	int complete;
};

struct top_state {
	enum cur_view view;
	struct top_action **tacts;
	char conn_status[TS_CONN_STATUS_LEN];
	int scroll_pos;
	int need_redraw;
	int done;
};

int init_top_state(struct top_state* ts, int test);
void clear_top_state(struct top_state *ts);
struct fscreen *write_fscreen(const struct top_state *ts);
void handle_keyboard_input(struct top_state *ts);

#endif
