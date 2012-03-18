/*
 * Copyright 2012 the Redfish authors
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

#ifndef REDFISH_OSD_OSTOR_DOT_H
#define REDFISH_OSD_OSTOR_DOT_H

#include <stdint.h> /* for uint64_t, etc. */

struct fast_log_buf;
struct ostor;
struct ostorc;

/* The object storage for the osd
 *
 * The OSD stores its objects on the local filesystem.  It makes an attempt to
 * keep recently used file descriptors open, to avoid the overhead of doing an
 * open() and a close() for each operation.
 */

extern struct ostor *ostor_init(const struct ostorc *oconf);

/** Shut down the object store.
 *
 * After this function has been called, all further calls to the ostor will
 * return ESHUTDOWN.
 *
 * @param ostor		The ostor
 */
extern void ostor_shutdown(struct ostor *ostor);

/** Free the object store
 *
 * Free the memory associated with the object store.
 *
 * @param ostor		The ostor
 */
extern void ostor_free(struct ostor *ostor);

/** Write to a chunk
 *
 * @param ostor		The ostor
 * @param fb		The fast log buffer
 * @param cid		The chunk ID
 * @param data		The data to write
 * @param dlen		Length of the data to write
 *
 * @return		0 on success; error code otherwise
 */
extern int ostor_write(struct ostor *ostor, struct fast_log_buf *fb,
		uint64_t cid, const char *data, uint32_t dlen);

/** Read from a chunk
 *
 * @param ostor		The ostor
 * @param fb		The fast log buffer
 * @param cid		The chunk ID
 * @param off		The offset to read from
 * @param data		(out param) The buffer to read into.  Must be at least
 *			dlen bytes long.
 * @param dlen		The amount to read
 *
 * @return		the number of bytes read on success; a negative error
 *			code otherwise
 */
extern int32_t ostor_read(struct ostor *ostor, struct fast_log_buf *fb,
		uint64_t cid, uint64_t off, char *data, uint32_t dlen);

/** Unlink a chunk
 *
 * After this call has returned, reads from the chunk will fail with -ENOENT.
 * If the chunk is written to afterwards, it will appear to start out empty.
 *
 * @param ostor		The ostor
 * @param fb		The fast log buffer
 * @param cid		The chunk ID
 *
 * @return		0 on success; error code otherwise
 */
extern int ostor_unlink(struct ostor *ostor, struct fast_log_buf *fb,
		uint64_t cid);

#endif
