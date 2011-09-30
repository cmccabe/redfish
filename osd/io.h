/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#ifndef ONEIFHS_CHUNK_IO_H
#define ONEIFHS_CHUNK_IO_H

#include <stdint.h>

#define MAX_CHUNK_SZ 134217728

int onechunk_set_prefix(const char *prefix);
int onechunk_write(uint64_t bid, const void *data, int count, int offset);
int onechunk_read(uint64_t bid, void *data, int count, int offset);

#endif
