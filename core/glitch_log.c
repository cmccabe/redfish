/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/glitch_log.h"
#include "core/log_config.h"
#include "util/error.h"
#include "util/safe_io.h"
#include "util/tempfile.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>

#define TEMP_GLITCH_LOG_SZ 16384

static pthread_mutex_t g_glitch_log_lock = PTHREAD_MUTEX_INITIALIZER;

static int g_configured = 0;

static int g_use_syslog = 0;

static int g_glitch_log_fd = -1;

static char g_glitch_log_fname[PATH_MAX] = { 0 };

void regurgitate_fd(char *line, size_t max_line, int ifd, int ofd,
		    int use_syslog)
{
	int ret;
	size_t bidx;
	ret = lseek(ifd, 0, SEEK_SET);
	if (ret)
		return;
	memset(line, 0, max_line);
	bidx = 0;
	while (1) {
		char b[1];
		int tmp, res = read(ifd, b, 1);
		if ((bidx == max_line - 2) || (res <= 0) || (b[0] == '\n')) {
			if (use_syslog) {
				syslog(LOG_ERR | LOG_USER, "%s", line);
			}
			if (b[bidx] == '\n')
				line[bidx++] = '\n';
			tmp = safe_write(STDERR_FILENO, line, bidx);
			if (ofd != -1)
				tmp = safe_write(ofd, line, bidx);
			if (res <= 0)
				break;
			memset(line, 0, max_line);
			bidx = 0;
		}
		else {
			line[bidx++] = b[0];
		}
	}
}

static void glitch_log_to_syslog_and_stderr(const char *buf, size_t buf_sz)
{
	int res;
	if (g_use_syslog)
		syslog(LOG_ERR | LOG_USER, "%s", buf);
	res = safe_write(STDERR_FILENO, buf, buf_sz);
}

static void glitch_log_impl(const char *fmt, va_list ap)
{
	char *buf;
	int ret, txt_sz = vsnprintf(NULL, 0, fmt, ap);
	buf = malloc(txt_sz + 1);
	if (!buf) {
		static const char OOM_MSG[] = "error writing to log: "
						"out of memory\n";
		glitch_log_to_syslog_and_stderr(OOM_MSG, sizeof(OOM_MSG) - 1);
		return;
	}
	vsnprintf(buf, txt_sz + 1, fmt, ap);
	if (g_glitch_log_fd != -1) {
		ret = safe_write(g_glitch_log_fd, buf, txt_sz);
		if (ret != 0) {
			char err[512];
			snprintf(err, sizeof(err), "error writing to log "
				 "file '%s': error %d\n",
				 g_glitch_log_fname, ret);
			glitch_log_to_syslog_and_stderr(err, strlen(err));
			g_glitch_log_fd = -1;
			g_glitch_log_fname[0] = '\0';
			RETRY_ON_EINTR(ret, close(g_glitch_log_fd));
		}
	}
	glitch_log_to_syslog_and_stderr(buf, txt_sz);
	free(buf);
}

static void open_temp_glitch_log(void)
{
	int ret;
	char tempdir[PATH_MAX];
	ret = get_tempdir(tempdir, PATH_MAX, 0755);
	if (ret)
		return;
	snprintf(g_glitch_log_fname, PATH_MAX, "%s/glitch_log.tmp.txt",
		 tempdir);
        RETRY_ON_EINTR(g_glitch_log_fd, open(g_glitch_log_fname,
			O_CREAT | O_RDWR, 0644));
	if (g_glitch_log_fd == -1) {
		g_glitch_log_fname[0] = '\0';
		remove_tempdir(tempdir);
		return;
	}
	register_tempdir_for_cleanup(tempdir);
}

void glitch_log(const char *fmt, ...)
{
	va_list ap;
	pthread_mutex_lock(&g_glitch_log_lock);
	if ((g_glitch_log_fd == -1) && (g_configured == 0)) {
		open_temp_glitch_log();
	}
	va_start(ap, fmt);
	glitch_log_impl(fmt, ap);
	va_end(ap);
	pthread_mutex_unlock(&g_glitch_log_lock);
}

void close_glitch_log(void)
{
	pthread_mutex_lock(&g_glitch_log_lock);
	if (g_glitch_log_fd != -1) {
		int res;
		RETRY_ON_EINTR(res, close(g_glitch_log_fd));
		g_glitch_log_fd = -1;
	}
	g_configured = 0;
	pthread_mutex_unlock(&g_glitch_log_lock);
}

void configure_glitch_log(const struct log_config *lc)
{
	int ret;
	int nfd = -1;

	pthread_mutex_lock(&g_glitch_log_lock);
	if (g_configured) {
		pthread_mutex_unlock(&g_glitch_log_lock);
		glitch_log("glitch log already configured.\n");
		return;
	}
	g_use_syslog = lc->use_syslog;
	if (lc->glitch_log) {
		RETRY_ON_EINTR(nfd, open(lc->glitch_log,
			O_WRONLY | O_CREAT | O_TRUNC, 0644));
		if (nfd == -1) {
			char err[512];
			ret = errno;
			snprintf(err, sizeof(err), "configure_glitch_log: "
				 "error opening '%s': error %d\n",
				 lc->glitch_log, ret);
			glitch_log_to_syslog_and_stderr(err, strlen(err));
		}
	}
	if (g_glitch_log_fd != -1) {
		char line[512], *last_slash;
		regurgitate_fd(line, sizeof(line), g_glitch_log_fd,
				nfd, g_use_syslog);
		RETRY_ON_EINTR(ret, close(g_glitch_log_fd));
		unlink(g_glitch_log_fname);
		last_slash = rindex(g_glitch_log_fname, '/');
		if (last_slash) {
			*last_slash = '\0';
			remove_tempdir(g_glitch_log_fname);
			unregister_tempdir_for_cleanup(g_glitch_log_fname);
		}
	}
	if (lc->glitch_log) {
		g_glitch_log_fd = nfd;
		snprintf(g_glitch_log_fname, PATH_MAX, lc->glitch_log);
	}
	else {
		g_glitch_log_fd = -1;
		g_glitch_log_fname[0] = '\0';
	}
	pthread_mutex_unlock(&g_glitch_log_lock);
}
