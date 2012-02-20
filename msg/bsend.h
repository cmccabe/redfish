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

#ifndef REDFISH_MSG_BRPC_H
#define REDFISH_MSG_BRPC_H

/*
 * Redfish blocking RPC implementation
 */

#include <stdint.h> /* for uint32_t, etc. */
#include <unistd.h> /* for size_t */

struct fast_log_buf;
struct msg;
struct msgr;
struct mtran;
struct bsend;

/** Blocking RPC flag: listen for a response to this message */
#define BSF_RESP 0x1

/** Create a blocking RPC sending context
 *
 * There is no timeout parameter here; timeouts are determined by the messengers
 * that you use to send messages.
 *
 * @param fb		Fast log buffer to use for the bsend operations
 * @param max_tr	Maximum simultaneous procedure calls that can be made at
 *			a time with this context
 *
 * @return		Pointer to a valid bsend_ctx, or an error pointer
 */
extern struct bsend *bsend_init(struct fast_log_buf *fb, int max_tr);

/** Send out an RPC message
 *
 * This function will allocate a transactor for you.
 *
 * @param ctx		The blocking RPC context
 * @param msgr		Messenger to send the message on
 * @param flags		Flags to use
 * @param msg		The message
 * @param addr		The destination address
 * @param port		The destination port
 * @param timeo		Timeout in seconds
 *
 * @return		0 on success; error code otherwise
 */
extern int bsend_add(struct bsend *ctx, struct msgr *msgr, uint8_t flags,
		struct msg *msg, uint32_t addr, uint16_t port, int timeo);

/** Send out an RPC message
 *
 * This function takes as a parameter an existing transactor.
 * If the add fails, the transactor will be freed using mtran_free.
 *
 * @param ctx		The blocking RPC context
 * @param msgr		Messenger to send the message on
 * @param flags		Flags to use
 * @param msg		The message
 * @param addr		The transactor to use
 * @param timeo		Timeout in seconds
 *
 * @return		0 on success; error code otherwise
 */
extern int bsend_add_tr_or_free(struct bsend *ctx, struct msgr *msgr,
		uint8_t flags, struct msg *msg, struct mtran *tr, int timeo);

/** Block until all the RPC calls have been made.
 *
 * This is a blocking call that will not return until all of the messages in the
 * pending list have been serviced, or the blocking RPC context's timeout has
 * elapsed.  If there are messages with BSF_RESP, we will wait for a response
 * from those.
 *
 * @param ctx		The blocking RPC context
 *
 * @return		The number of RPC calls made.  Even RPCs that resulted
 *			in an error are counted.
 */
extern int bsend_join(struct bsend *ctx);

/** Access an mtran
 *
 * This must be invoked after bsend_join.
 *
 * If a response is an ERR_PTR(ETIMEDOUT), that means that we timed out
 * waiting for a response.  If a response is some other error pointer, that
 * means that error occured during the communication.  The response message will
 * be NULL for any message _successfully_ sent without the BSF_RESP flag.  You
 * can still get errors when sending messages without BSF_RESP.  Basically, you
 * are getting back the error of the TCP send operation.
 *
 * @param ctx		The blocking RPC context
 * @param ntr		The mtran to get
 *
 * @return		the mtran, or an error pointer on error
 */
extern struct mtran *bsend_get_mtran(struct bsend *ctx, int ntr);

/** Free all messages remaining from an RPC session
 *
 * After this is called, we can call bsend_add again to again begin making RPCs
 * with this context.
 *
 * @param ctx		The blocking RPC context
 */
extern void bsend_reset(struct bsend *ctx);

/** Free a blocking RPC context
 *
 * Must not be used on active contexts!
 *
 * @param ctx		The blocking RPC context
 */
extern void bsend_free(struct bsend *ctx);

#endif
