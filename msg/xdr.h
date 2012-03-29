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

#ifndef REDFISH_MSG_XDR_DOT_H
#define REDFISH_MSG_XDR_DOT_H

/*
 * Redfish XDR convenience functions
 */

#include <stdint.h> /* for uint32_t, etc. */
#include <unistd.h> /* for size_t */
#include <rpc/xdr.h> /* for xdrproc_t */

/** Allocate a message with an XDR payload
 *
 * @param ty		The message type
 * @param xdrproc	The serialization function to use
 * @param payload	The data to serialize
 *
 * @return		the message, or an error pointer on error 
 */
extern struct msg* msg_xdr_alloc(uint16_t ty, xdrproc_t xdrproc,
		void *payload);

/** Allocate a message with an XDR payload followed by some extra space
 *
 * @param ty		The message type
 * @param xdrproc	The serialization function to use
 * @param payload	The data to serialize
 * @param extra_len	Extra length to reserve
 * @param extra		(out param) pointer to extra space
 *
 * @return		the message, or an error pointer on error 
 */
extern struct msg* msg_xdr_extalloc(uint16_t ty, xdrproc_t xdrproc,
		void *payload, size_t extra_len, void **extra);

/** Decode a message with an XDR payload followed by some extra space
 *
 * @param xdrproc	The deserialization function to use
 * @param m		The message
 * @param out		The structure to decode into.
 *			You are responsible for allocating this structure
 *			(usually by declaring it on the stack).
 *			You must call xdr_free on this object later if we
 *			successfully fill it.
 * @param extra		(out param) pointer to extra space
 *
 * @return		negative error code on error, or the number of extra
 *			bytes at the end of this message.
 */
extern int32_t msg_xdr_extdecode(xdrproc_t xdrproc,
	const struct msg *m, void *out, const void **extra);

/** Decode a message with an XDR payload
 *
 * @param xdrproc	The deserialization function to use
 * @param m		The message
 * @param out		The structure to decode into.
 *			You are responsible for allocating this structure
 *			(usually by declaring it on the stack).
 *			You must call xdr_free on this object later if we
 *			successfully fill it.
 *
 * @return		negative error code on error, or the number of extra
 *			bytes at the end of this message.
 */
extern int32_t msg_xdr_decode(xdrproc_t xdrproc,
		const struct msg *m, void *out);

/** Allocate a message on the heap
 *
 * @param t		The message type
 * @param payload	Payload to include in the message
 *
 * @return		A message, or an error pointer on error.
 *			The message must be freed with msg_release
 */
#define MSG_XDR_ALLOC(t, payload) \
	(msg_xdr_alloc(t##_ty, (xdrproc_t)xdr_##t, payload))

/** Deserialize a message into a response struct
 *
 * Basically, this is a wrapper around xdr_free that provides typechecking.
 *
 * @param t		The message type
 * @param m		The message
 * @param out		(out param) The response structure.
 *			This must be freed with XDR_RESP_FREE.
 *
 * @return		A negative error code on error, 0 or higher otherwise
 */
#define MSG_XDR_DECODE(t, m, out) \
	(msg_xdr_decode((xdrproc_t)xdr_##t, m, out))

/** Free a message allocated with msg_xdr_alloc.
 *
 * Basically, this is a wrapper around xdr_free that provides typechecking.
 *
 * @param t		The message type
 * @param payload	The payload to free
 */
#define XDR_REQ_FREE(t, payload) do { \
	struct t *__p = payload; \
	xdr_free((xdrproc_t)xdr_##t, (void*)__p); \
} while (0);

/** Try to decode a message as a generic response
 *
 * @param m		The message
 *
 * @return		Negative error code if the message is not a resp;
 *			The error code embedded in the resp message otherwise
 *			(which may be positive or zero).
 */
extern int32_t msg_xdr_decode_as_generic(const struct msg *m);

#endif
