/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "top/fscreen.h"

#include <curses.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

struct fscreen *fscreen_init(void)
{
	struct fscreen *sn;
	sn = calloc(1, sizeof(struct fscreen));
	return sn;
}

void fscreen_wipelines(struct fscreen *sn)
{
	int i;
	for (i = 0; i < sn->num_lines; ++i) {
		fscreen_freeline(sn->lines[i]);
	}
	sn->num_lines = 0;
	free(sn->lines);
	sn->lines = NULL;
}

void fscreen_free(struct fscreen *sn)
{
	if (sn->header)
		fscreen_freeline(sn->header);
	if (sn->footer)
		fscreen_freeline(sn->footer);
	fscreen_wipelines(sn);
	free(sn);
}

static void print_spaces(int size)
{
	char buf[size + 1];
	memset(buf, ' ', sizeof(buf) - 1);
	buf[size] = '\0';
	printw("%s", buf);
}

static void draw_line(int sy, int mx, const struct fscreen_line *l)
{
	struct fscreen_line_segment *s;
	int res, cps, slop, nsegs = 0;
	for (s = l->segs; s; s = s->next) {
		nsegs++;
	}
	cps = mx / nsegs; /* calc max characters per segment */
	move(sy, 0); /* move cursor to current line */
	/* now draw each line segment */
	for (s = l->segs; s; s = s->next) {
		int seglen, soff;

		seglen = strlen(s->text);
		if (seglen > cps - 2)
			seglen = cps - 2;
		soff = (cps - seglen) / 2;

		/* output beginning spacer */
		res = attrset(l->attrbits);
		print_spaces(soff);

		/* output text */
		res = attrset(s->attrbits);
		addnstr(s->text, seglen);

		/* output ending spacer */
		res = attrset(l->attrbits);
		print_spaces(cps - (seglen + soff));
	}
	slop = mx - (cps * nsegs);
	if (slop > 0) {
		print_spaces(slop);
	}
}

void fscreen_draw(const struct fscreen *sn)
{
	int y, mx = 80, my = 25;
	getmaxyx(stdscr, my, mx);  /* get the new screen size */
	if ((mx < 5) || (my < 5))
		return;

	/* draw */
	draw_line(0, mx, sn->header);
	for (y = 0; y < my; ++y) {
		int idx = y + sn->scroll_pos;
		if (idx >= sn->num_lines) {
			move(y, 0);
			addch('~');
			print_spaces(mx -1);
		}
		else {
			draw_line(1 + y, mx, sn->lines[idx]);
		}
	}
	draw_line(my - 1, mx, sn->footer);
}

struct fscreen_line* fscreen_makeline(int attrbits, ...)
{
	struct fscreen_line_segment **seg;
	struct fscreen_line *line;
	char *str;
	int abits;
	va_list ap;

	line = calloc(1, sizeof(struct fscreen_line));
	if (!line)
		return NULL;
	line->attrbits = attrbits;
	va_start(ap, attrbits);
	seg = &line->segs;
	while (1) {
		str = va_arg(ap, char*);
		if (!str) {
			va_end(ap);
			return line;
		}
		abits = va_arg(ap, int);
		*seg = calloc(1, sizeof(struct fscreen_line_segment));
		if (!*seg) {
			va_end(ap);
			fscreen_freeline(line);
			return NULL;
		}
		(*seg)->text = strdup(str);
		if (!(*seg)->text) {
			va_end(ap);
			fscreen_freeline(line);
			return NULL;
		}
		(*seg)->attrbits = abits;
		(*seg)->next = NULL;
		seg = &(*seg)->next;
	}
}

void fscreen_freeline(struct fscreen_line* line)
{
	struct fscreen_line_segment *seg, *nseg;
	seg = line->segs;
	while (seg) {
		nseg = seg->next;
		free(seg->text);
		free(seg);
		seg = nseg;
	}
	free(line);
}


int fscreen_scroll_constrain(const struct fscreen *sn, int scroll_pos)
{
	int mx = 80, my = 25, ey;
	getmaxyx(stdscr, my, mx);  /* get the new screen size */
	if ((mx < 5) || (my < 5))
		return 0;
	ey = my - 2;

	/* correct scroll position */
	if (scroll_pos < 0)
		scroll_pos = 0;
	if (scroll_pos + ey > sn->num_lines) {
		if (sn->num_lines <= ey) {
			scroll_pos = 0;
		}
		else {
			scroll_pos = sn->num_lines - ey;
		}
	}
	return scroll_pos;
}
