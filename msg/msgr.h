/*
 * The RedFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
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
 * Receive event: 'tr->m' will be the message that was just received. The message
 * was allocated with malloc and you are responsible for freeing it.
 * Call mtran_send_next if you want to send a reply to what you heard.
 *
 * Send success event: 'tr->m' will be NULL.
 * Call mtran_recv_next if you want to listen for a reply to what you sent.
 *
 * Send failure event: 'tr->m' will be an error ptr containing the errno failure
 * code.  See util/error.h for details about error pointers.
 *
 * If you don't call mtran_*_next, the messenger doesn't have anything more to
 * do with your tranactor. You should probably either free it with mtran_free,
 * or pass it to some other subsystem that cares about it.
 */
typedef void (*msgr_cb_t)(struct mconn *conn, struct mtran *tr);

/** Initialize the messenger.
 *
 * @param err		(out param) error buffer
 * @param err_len	length of err
 * @param max_conn	Maximum number of connections to allow.
 * @param max_tran	Maximum number of simultaneous transactions to allow
 * @param tran_sz	Size to use when allocating mtran objects. Should be
 *			 at least sizeof(struct mtran), but it may be much
 *			 more if you want to store your own state in the
 *			 transactor structure.
 * @param cb		Callback to invoke when a complete message is sent or
 *			 received.
 * @param mgr		Fast log manager to use for fast logs
 *
 * @return		the messenger on success; NULL otherwise
 */
extern struct msgr *msgr_init(char *err, size_t err_len,
		int max_conn, int max_tran, size_t tran_sz,
		msgr_cb_t cb, struct fast_log_mgr *mgr);

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

/** Shutdown a messenger
 *
 * @param msgr		The messenger
 */
extern void msgr_shutdown(struct msgr *msgr);

#endif
