/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/signal.h"
#include "jorm/json.h"
#include "mon/output_worker.h"
#include "top/fscreen.h"
#include "top/state.h"
#include "util/error.h"
#include "util/platform/pipe2.h"
#include "util/safe_io.h"
#include "util/string.h"

#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int g_winch_fd[2] = { -1, -1 };

static void usage(int exitstatus)
{
	static const char *usage_lines[] = {
"fishtop: the OneFish cluster monitor view.",
"See http://www.club.cc.cmu.edu/~cmccabe/onefish.html for the most up-to-date",
"information about OneFish.",
"",
"fishtop is a curses-based cluster monitor viewer. You would normally not run ",
"it directly, but instead invoke it through fishmon. See the help for fishmon.",
NULL
	};
	print_lines(stderr, usage_lines);
	exit(exitstatus);
}

static void parse_arguments(int argc, char **argv, int *comm_fd, int *color)
{
	int c;

	while ((c = getopt(argc, argv, "f:hNT")) != -1) {
		switch (c) {
		case 'f':
			/* N.B. comm_fd can't be 0 because stdin is used by curses */
			*comm_fd = atoi(optarg);
			if (*comm_fd <= 0) {
				fprintf(stderr, "invalid argument for comm_fd.\n");
				usage(EXIT_FAILURE);
			}
			break;
		case 'h':
			usage(EXIT_SUCCESS);
		case 'N':
			*color = 0;
			break;
		case 'T':
			*comm_fd = -1;
			break;
		case '?':
			fprintf(stderr, "error parsing options.\n\n");
			usage(EXIT_FAILURE);
		}
	}
	if (*comm_fd == 0) {
		fprintf(stderr, "You must specify a communication fd "
			"with -f.\n");
		usage(EXIT_FAILURE);
	}
}

static void handle_curses_window_resize(POSSIBLY_UNUSED(int sig))
{
	int tmp = 0, ret;
	ret = safe_write(g_winch_fd[PIPE_WRITE], &tmp, 1);
}

static void do_curses_init(int color)
{
	initscr();
	scrollok(stdscr, FALSE);
	keypad(stdscr, TRUE);
	nonl();
	cbreak();
	noecho();
	if ((color) && (has_colors())) {
		start_color();
		init_pair(1, COLOR_RED, COLOR_WHITE);
		init_pair(2, COLOR_GREEN, COLOR_WHITE);
		init_pair(3, COLOR_YELLOW, COLOR_WHITE);
		init_pair(4, COLOR_BLUE, COLOR_WHITE);
		init_pair(5, COLOR_CYAN, COLOR_WHITE);
		init_pair(6, COLOR_MAGENTA, COLOR_WHITE);
		init_pair(7, COLOR_BLACK,COLOR_WHITE);
	}
	nodelay(stdscr, TRUE); /* set non-blocking input */
}

static void do_endwin(POSSIBLY_UNUSED(int sig))
{
	endwin();
}

static struct json_object *read_next_json(int fd, char *err, size_t err_len)
{
	int ret;
	struct json_object *jo;
	char *mbuf, lbuf[LBUF_LEN_DIGITS + 2] = { 0 };
	uint32_t msg_len;

	ret = safe_read_exact(fd, lbuf, LBUF_LEN_DIGITS);
	if (ret) {
		snprintf(err, err_len, "read error %d", ret);
		return NULL;
	}
	if (lbuf[0] != '\n') {
		snprintf(err, err_len, "bad first msg byte");
		return NULL;
	}
	if (strspn(lbuf + 1, " 0123456789") != LBUF_LEN_DIGITS) {
		snprintf(err, err_len, "bad msg length");
		return NULL;
	}
	msg_len = atoi(lbuf + 1);
	if (msg_len == 0) {
		snprintf(err, err_len, "invalid msg length");
		return NULL;
	}
	mbuf = malloc(msg_len + 1);
	if (!mbuf) {
		snprintf(err, err_len, "out of memory");
		return NULL;
	}
	ret = safe_read_exact(fd, mbuf, msg_len);
	if (ret) {
		free(mbuf);
		snprintf(err, err_len, "msg read error %d", ret);
		return NULL;
	}
	mbuf[msg_len] = '\0';
	jo = parse_json_string(mbuf, err, err_len);
	free(mbuf);
	if (err[0])
		return NULL;
	return jo;
}

static void handle_json_message(struct json_object *jo, POSSIBLY_UNUSED(struct top_state *ts))
{
	// TODO: handle 'closing' JSON message
	json_object_put(jo);
}

static void main_loop(int comm_fd, char *prog_err, size_t prog_err_len)
{
	FILE *fp;
	int ret;
	struct top_state ts;
	ret = init_top_state(&ts, (comm_fd == -1));
	if (ret) {
		snprintf(prog_err, prog_err_len,
			 "init_top_state got error %d", ret);
		goto done;
	}
	fp = fopen("/tmp/out", "w");
	setvbuf(fp, NULL, _IONBF, 0);
	while (1) {
		struct pollfd pfd[3];
		int res, npfd = 0;
		char err[512] = { 0 };

		memset(&pfd, 0, sizeof(pfd));
		pfd[npfd].fd = STDIN_FILENO;
		pfd[npfd++].events = POLLIN;
		pfd[npfd].fd = g_winch_fd[PIPE_READ];
		pfd[npfd++].events = POLLIN;
		if (comm_fd > 0) {
			pfd[npfd].fd = comm_fd;
			pfd[npfd++].events = POLLIN;
		}

		fprintf(fp, "running poll...\n");
		RETRY_ON_EINTR(res, poll(pfd, npfd, -1));
		if (res < 0) {
			snprintf(prog_err, prog_err_len, "poll error %d",
				 res);
			break;
		}
		if (pfd[0].revents & POLLIN) {
			fprintf(fp, "got STDIN_FILENO entry\n");
			handle_keyboard_input(&ts);
		}
		if (pfd[1].revents & POLLIN) {
			int tmp = 0;
			fprintf(fp, "got g_winch_fd entry\n");
			res = safe_read(g_winch_fd[PIPE_READ], &tmp, 1);
			ts.need_redraw = 1;
		}
		if (pfd[2].revents & POLLIN) {
			struct json_object *jo;
			jo = read_next_json(comm_fd, err, sizeof(err));
			if (err[0]) {
				snprintf(ts.conn_status, TS_CONN_STATUS_LEN,
					 "Disconnected: %s", err);
				fprintf(fp, "got comm_fd err %s\n", err);
			}
			else {
				handle_json_message(jo, &ts);
				fprintf(fp, "handle_json_message\n");
			}
		}

		if (ts.done)
			break;
		if (ts.need_redraw) {
			struct fscreen *sn = write_fscreen(&ts);
			if (!sn) {
				snprintf(prog_err, prog_err_len,
					"write_fscreen ran out of memory");
				goto done_clear_top_state;
			}
			ts.scroll_pos = fscreen_scroll_constrain(sn, ts.scroll_pos);
			sn->scroll_pos = ts.scroll_pos;
			fscreen_draw(sn);
			fscreen_free(sn);
			refresh();
		}
	}
	fclose(fp);
done_clear_top_state:
	clear_top_state(&ts);
done:
	return;
}

int main(int argc, char **argv)
{
	int res, ret, tmp = 0, comm_fd = 0, color = 1;
	char err[512] = { 0 };

	parse_arguments(argc, argv, &comm_fd, &color);
	signal_init(err, sizeof(err), NULL, do_endwin);
	if (err[0]) {
		fprintf(stderr, "signal_init error: %s\n", err);
		return EXIT_FAILURE;
	}
	ret = pipe2(g_winch_fd, O_CLOEXEC | O_NONBLOCK);
	if (ret) {
		fprintf(stderr, "unable to create winch pipes! "
			"error %d\n", ret);
		return EXIT_FAILURE;
	}
	do_curses_init(color);
	signal(SIGWINCH, handle_curses_window_resize);

	/* send the initial draw screen request */
	ret = safe_write(g_winch_fd[PIPE_WRITE], &tmp, 1);
	main_loop(comm_fd, err, sizeof(err));
	signal(SIGWINCH, SIG_DFL);
	endwin();
	if (err[0]) {
		fprintf(stderr, "%s\n", err);
		ret = EXIT_FAILURE;
	}
	else {
		ret = EXIT_SUCCESS;
	}

	signal(SIGWINCH, SIG_DFL);
	RETRY_ON_EINTR(res, close(g_winch_fd[PIPE_WRITE]));
	RETRY_ON_EINTR(res, close(g_winch_fd[PIPE_READ]));
	return 0;
}
