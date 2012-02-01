/*
 * Copyright 2011-2012 the Redfish authors
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

#ifndef REDFISH_MSG_MSGR_DOT_H
#define REDFISH_MSG_MSGR_DOT_H

#include <stdint.h> /* for uint32_t, etc. */
#include <unistd.h> /* for size_t */

struct mconn;
struct msg;
struct msgr;
struct mtran;

/* The messenger
 *
 * Each messenger has a single thread which is handling potentially thousands of
 * TCP sockets at once. All network I/O is nonblocking. On Linux, we use epoll to
 * handle all these sockets.
 *
 * To use the messenger, you need to structure your code in terms of
 * 'transactors.' Each transactor represents an ongoing transaction.  The
 * messenger will execute callbacks whenever a complete message is sent or
 * received on a connection. It will deal with the tedious details of the socket
 * API like partial reads and writes.
 *
 * All callbacks happen in the context of the messenger thread. That means that
 * we do not need to lock any of the messenger data structures for the duration
 * of the callback. It also means that callbacks should never perform blocking
 * I/O or perform an excessive amount of computation.
 *
 * Opening new TCP sockets is expensive in terms of latency, because of the
 * overhead of the 3-way handshake and other things. So we open a connection
 * (mconn) to service one transactor, we will keep it open and potentially use
 * it for other transactors. The messenger handles all these details behind the
 * scenes. If there is a network problem, the messenger will invoke the callback
 * with an error pointer set to the errno code.
 */


/** Transactor callback. This is called whenever a message is sent or received
 * by the messenger.
 *
 * On a receive completed event, tr->state == MTRAN_STATE_RECV.
 * On a send completed event, tr->state == MTRAN_STATE_SENT.
 * See msg/msg.h for a description of the tr->m field.
 *
 * If you don't call mtran_*_next, the messenger doesn't have anything more to
 * do with your tranactor. You should probably either free it with mtran_free,
 * or pass it to some other subsystem that cares about it.
 */
typedef void (*msgr_cb_t)(struct mconn *conn, struct mtran *tr);

/** Initialize the messenger.
 *
 * @param err			(out param) error buffer
 * @param err_len		length of err
 * @param max_conn		Maximum number of connections to allow.
 * @param max_tran		Maximum number of simultaneous transactors to
 *				allow
 * @param cb			Callback to invoke when a complete message is
 *				sent or received.
 * @param timeout_period	Timeout period in seconds
 * @param timeout_cnt_max	Number of timeout periods to allow to expire
 *				before timing out a connection or transactor.
 * @param mgr			Fast log manager to use for fast logs
 *
 * @return			the messenger on success; NULL otherwise
 */
extern struct msgr *msgr_init(char *err, size_t err_len,
		int max_conn, int max_tran,
		msgr_cb_t cb, int timeout_period, int timeout_cnt_max,
		struct fast_log_mgr *mgr);

/** Configure the messenger to listen on a given TCP port.
 *
 * @param msgr		the messenger
 * @param port		TCP port to listen on
 * @param err		(out param) error buffer
 * @param err_len	length of err
 *
 * @return		the messenger on success; NULL otherwise
 */
extern void msgr_listen(struct msgr *msgr, uint16_t port,
		char *err, size_t err_len);

/** Start the messenger.
 *
 * This starts the messenger thread.  If you have configured the messenger to
 * listen for incoming connections, they will begin to arrive once you call this
 * function.
 *
 * @param msgr		the messenger
 * @param err		(out param) error buffer
 * @param err_len	length of err
 *
 * @return		the messenger on success; NULL otherwise
 */
extern void msgr_start(struct msgr *msgr, char *err, size_t err_len);

/** Allocate a new messenger transactor.
 *
 * We zero-initialize the memory.
 *
 * @param msgr		the messenger
 *
 * @return		A new messenger transactor, or NULL on OOM
 */
extern void *mtran_alloc(struct msgr* msgr);

/** Free a messenger transactor
 *
 * @param tr		The messenger transactor to free
 */
extern void mtran_free(struct mtran *tr);

/** Queue a message for sending
 *
 * If there is a pre-existing connection to the remote host, it will be used.
 * Otherwise, a new connection will be opened.
 *
 * This can be called from any context.
 *
 * @param msgr		the messenger
 * @param tr		the transactor
 * @param addr		Remote IP address
 * @param port		Remote port
 * @param m		The message to send.
 * 			This must be dynamically allocated. The messenger will
 * 			take ownership of this pointer and free it later.
 */
extern void mtran_send(struct msgr *msgr, struct mtran *tr,
		uint32_t addr, uint16_t port, struct msg *m);

/** Queue a message for sending on a currently open connection
 *
 * This must be called from the context of a msgr_cb_t function.
 *
 * @param conn		the connection that the transactor is associated with
 * @param tr		the transactor
 * @param m		The message to send.
 * 			This must be dynamically allocated. The messenger will
 * 			take ownership of this pointer and free it later.
 */
extern void mtran_send_next(struct mconn *conn, struct mtran *tr,
			struct msg *m);

/** Register to receive a message from a currently open connection
 *
 * This must be called from the context of a msgr_cb_t function.
 *
 * @param conn		the connection that the transactor is associated with
 * @param tr		the transactor
 */
extern void mtran_recv_next(struct mconn *conn, struct mtran *tr);

/** Shut down a messenger.
 *
 * Shutdown will close all open connections and join the messenger thread.
 * Currently pending messages will receive ECANCELED.  Further attempts to send
 * messages through the messenger will result in ECANCELED getting delivered
 * immediately.
 *
 * @param msgr		The messenger
 */
extern void msgr_shutdown(struct msgr *msgr);

/** Free the memory associated with a messenger.
 *
 * You can only free messengers that have been shut down.  Before you free a
 * messenger, you must make absolutely sure that no threads are referencing it.
 * Most likely this means you will need to join those threads.
 *
 * @param msgr		The messenger
 */
extern void msgr_free(struct msgr *msgr);

#endif
