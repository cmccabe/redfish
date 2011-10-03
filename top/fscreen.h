/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef REDFISH_TOP_FSCREEN_DOT_H
#define REDFISH_TOP_FSCREEN_DOT_H

struct fscreen_line_segment {
	struct fscreen_line_segment *next;
	char *text;
	int attrbits;
};

struct fscreen_line {
	int attrbits;
	struct fscreen_line_segment *segs;
};

struct fscreen {
	struct fscreen_line *header;
	struct fscreen_line *footer;
	struct fscreen_line **lines;
	int num_lines;
	int scroll_pos;
};

/* Initialize an fscreen object
 *
 * @returns		NULL on OOM, or an fscreen object
 */
struct fscreen *fscreen_init(void);

/* Clear and free all lines in an fscreen object
 *
 * (But not the header and footer)
 *
 * @param sn		The fscreen
 */
void fscreen_wipelines(struct fscreen *sn);

/* Free an fscreen object, including the lines and header/footer
 *
 * Also frees the fscreen object itself.
 *
 * @param sn		The fscreen
 */
void fscreen_free(struct fscreen *sn);

/* Draw an fscreen object using curses
 *
 * @param sn		The fscreen to draw
 */
void fscreen_draw(const struct fscreen *sn);

/* Create an fscreen line
 *
 * @param attrbits	The background attributes for this line
 * @param ...		A series that looks like this:
 * 			[repeated one or more times] {
 * 				- char* text
 *				- int attrbits
 *			}
 * 			- a terminating NULL
 * 			Each attribts/text pair represents a string and the
 * 			style it should be drawn in.
 *
 * @return		NULL on OOM; an fscreen_line object otherwise.
 *			All strings will be dynamically allocated copies of
 *			whatever is passed in.
 */
struct fscreen_line* fscreen_makeline(int attrbits, ...);

/* Free an fscreen line
 *
 * @param line		The fscreen line to free.
 */
void fscreen_freeline(struct fscreen_line* line);

/* Constrain the scroll position using the current screen size and fscreen
 * contents
 *
 * @param sn		The fscreen
 * @param scoll_pos	Current scroll position
 *
 * @return		New scroll position
 */
int fscreen_scroll_constrain(const struct fscreen *sn, int scroll_pos);

#endif
