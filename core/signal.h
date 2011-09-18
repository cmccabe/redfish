/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEFISH_CORE_SIGNAL_DOT_H
#define ONEFISH_CORE_SIGNAL_DOT_H

#include <unistd.h> /* for size_t */

typedef void (*signal_cb_t)(int);

/** Install the signal handlers for a OneFish daemon.
 *
 * @param error			a buffer to write any errors to
 * @param error_len		length of the error buffer
 * @param crash_log		The crash log to write when a fatal signal
 *				happens. If this is NULL, we'll write the crash
 *				log to stderr.
 * @param fatal_signal_cb	Callback that is executed after a fatal signal,
 *				or NULL for none.
 *
 * We write out an error message to error if signal_init fails.
 */
void signal_init(char *error, size_t error_len, const char *crash_log,
		 signal_cb_t fatal_signal_cb);

/** Clear all signal handlers, free the alternate signal stack, and disable the
 * crash log.
 */
void signal_resset_dispositions(void);

#endif
