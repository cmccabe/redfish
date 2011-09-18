/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_TOP_GFX_DOT_H
#define ONEFISH_TOP_GFX_DOT_H

struct fscreen;
struct top_action;
struct top_state;

struct fscreen* gfx_fscreen_create(const struct top_state *ts);
int gfx_set_test_lines(struct fscreen *sn);
int gfx_top_actions_to_fscreen(struct top_action **tacts, struct fscreen *sn);

#endif
