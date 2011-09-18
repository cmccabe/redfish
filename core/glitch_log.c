/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/glitch_log.h"
#include "core/log_config.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>

static pthread_mutex_t g_glitch_log_lock;

static FILE* g_glitch_log_fp;

void open_glitch_log(const struct log_config *lconf, char *err, size_t err_len)
{
	FILE *fp;
	int ret;
	ret = pthread_mutex_init(&g_glitch_log_lock, NULL);
	if (ret) {
		snprintf(err, err_len, "pthread_mutex_init error %d\n", ret);
		return;
	}
	if (lconf->glitch_log == NULL) {
		g_glitch_log_fp = NULL;
		return;
	}
	fp = fopen(lconf->glitch_log, "w");
	if (fp == NULL) {
		ret = errno;
		snprintf(err, err_len, "error opening '%s': error %d\n",
			 lconf->glitch_log, ret);
		pthread_mutex_destroy(&g_glitch_log_lock);
		return;
	}
	g_glitch_log_fp = fp;
	return;
}

/* TODO: also issue syslog here, if configured */
void glitch_log(const char *fmt, ...)
{
	int res;
	va_list ap;

	pthread_mutex_lock(&g_glitch_log_lock);
	if (g_glitch_log_fp == NULL) {
		pthread_mutex_unlock(&g_glitch_log_lock);
		return;
	}
	va_start(ap, fmt);
	res = vfprintf(g_glitch_log_fp, fmt, ap);
	va_end(ap);
	if (res < 0) {
		fclose(g_glitch_log_fp);
		g_glitch_log_fp = NULL;
	}
	pthread_mutex_unlock(&g_glitch_log_lock);
}

void close_glitch_log(void)
{
	fclose(g_glitch_log_fp);
	g_glitch_log_fp = NULL;
	pthread_mutex_destroy(&g_glitch_log_lock);
}
