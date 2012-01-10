/*
 * Copyright 2011-2012 the RedFish authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef REDFISH_UTIL_SAFE_IO
#define REDFISH_UTIL_SAFE_IO

#include "util/compiler.h"

#include <unistd.h>

ssize_t safe_write(int fd, const void *b, size_t c) WARN_UNUSED_RES;
ssize_t safe_pwrite(int fd, const void *b, size_t c, off_t off) WARN_UNUSED_RES;

ssize_t safe_read(int fd, void *b, size_t c) WARN_UNUSED_RES;
ssize_t safe_pread(int fd, void *b, size_t c, off_t off) WARN_UNUSED_RES;
ssize_t safe_read_exact(int fd, void *b, size_t c) WARN_UNUSED_RES;
ssize_t safe_pread_exact(int fd, void *b, size_t c, off_t off) WARN_UNUSED_RES;

int safe_close(int fd);

#endif
