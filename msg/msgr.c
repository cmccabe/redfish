/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "msg/fast_log.h"
#include "msg/generic.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_types.h"
#include "util/macro.h"
#include "util/net.h"
#include "util/platform/socket.h"
#include "util/queue.h"
#include "util/thread.h"
#include "util/tree.h"

#include <arpa/inet.h>
#include <errno.h>
#include <ev.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/****************************** prototypes ********************************/
static int mtran_compare(struct mtran *tr_a, struct mtran *tr_b);
static int mconn_compare(struct mconn *a, struct mconn *b);
static void mconn_teardown(struct mconn *conn, int failcode);
static void mconn_writable_cb(struct ev_loop *loop, struct ev_io *w,
		int revents);
static void mconn_readable_cb(struct ev_loop *loop, struct ev_io *w,
		int revents);
static void run_msgr_timeout_cb(POSSIBLY_UNUSED(struct ev_loop *loop),
               struct ev_timer *w, int revents);
static void run_msgr_notify_cb(struct ev_loop *loop, struct ev_async *w,
		int revents);
static void mconn_next_state_logic(struct mconn *conn);

/****************************** types ********************************/
enum mconn_state_t {
	MCONN_CONNECTING = 0,
	MCONN_QUIESCENT,
	MCONN_WRITING,
	MCONN_AWAITING_HDR,
	MCONN_READING_HDR,
	MCONN_READING,
	MCONN_NUM_STATES,
};

BUILD_BUG_ON(MCONN_NUM_STATES > 16);

STAILQ_HEAD(pending_tr, mtran);
RB_HEAD(active_tr, mtran);
RB_GENERATE(active_tr, mtran, u.active_entry, mtran_compare);

struct mconn {
	RB_ENTRY(mconn) entry;
	/** The messenger this connection is associated with */
	struct msgr *msgr;
	/** remote IP address */
	uint32_t ip;
	/** remote port */
	uint16_t port;
	/** state of this connection */
	uint16_t state;
	/** the socket, if connected. -1 if not */
	int sock;
	/** number of bytes sent/received, or -1 if not sending/receiving */
	int cnt;
	/** message that we're in the middle of reading, or NULL */
	struct msg *inbound_msg;
	/** transaction that we're in the middle of reading, or NULL */
	struct mtran *inbound_tr;
	/** writable event watcher for sock */
	struct ev_io w_write;
	/** readable event watcher for sock */
	struct ev_io w_read;
	/** Active transactions. Keyed on transaction id. */
	struct active_tr active_head;
	/** Pending transactions */
	struct pending_tr pending_head;
	/** Number of timeout periods that have passed without any TCP traffic */
	int timeout_cnt;
};

enum msgr_state_t {
	MSGR_STATE_INIT,
	MSGR_STATE_THREAD_STARTED,
	MSGR_STATE_THREAD_STOPPING,
};

RB_HEAD(msgr_conn, mconn);
RB_GENERATE(msgr_conn, mconn, entry, mconn_compare);

struct msgr {
	/** lock that protects thread state and pending_head */
	pthread_spinlock_t lock;
	/** thread state */
	enum msgr_state_t state;
	/** thread */
	struct redfish_thread rt;
	/** Main loop */
	struct ev_loop *loop;
	/** Socket we're listening on, or -1 if we're not listening on any
	 * sockets */
	int listen_fd;
	/** Port we're listening on, if any */
	uint16_t listen_port;
	/** Size to use when allocating mtran objects on the heap */ 
	size_t tran_sz;
	/** Callback to invoke when transactions receive messages */
	msgr_cb_t cb;
	/** Next transaction ID that will be given out */
	uint32_t next_trid;
	/** Current number of transactions we're tracking */
	int cur_tran;
	/** Maximum number of transactions we'll track */
	int max_tran;
	/** Current number of connections */
	int cur_conn;
	/** Maximum number of simultaneous connections to allow */
	int max_conn;
	/** TCP connections. Keyed on remote IP address */
	struct msgr_conn conn_head;
	/** Watches listen_fd */
	struct ev_io w_listen_fd;
	/** Async watcher. Lets us know that another thread asked us to shut
	 * down or send a message. */
	struct ev_async w_notify;
	/** event watcher for connection timeout */
	struct ev_timer w_timeout;
	/** Pending transactions not yet assigned to a connection */
	struct pending_tr pending_tr_head;
	/** Fast log buffer manager */
	struct fast_log_mgr *fb_mgr;
	/** Timeout period length, in seconds */
	int timeout_period;
	/** Maximum number of timeout periods to wait for before timing out a
	 * connection or transactor */
	int timeout_cnt_max;
};

/****************************** utility ********************************/
static uint16_t cram_into_u16(int val)
{
	if (val > 0xfffe)
		return 0xfffe;
	if (val < 0)
		return 0xffff;
	return val;
}

static int is_temporary_socket_error(int err)
{
	return ((err == EAGAIN) || (err == EWOULDBLOCK) || (err == EINTR));
}

void fast_log_msgr(struct msgr *msgr, uint32_t ty,
		uint16_t port, uint32_t ip,
		uint32_t trid, uint32_t rem_trid, uint16_t event,
		uint16_t event_data)
{
	fast_log_msgr_impl(msgr->rt.fb, ty,
		port, ip, trid, rem_trid, event, event_data);
}

/****************************** mtran ********************************/
void *mtran_alloc(struct msgr *msgr)
{
	struct mtran *tr = calloc(1, msgr->tran_sz);
	if (!tr)
		return NULL;
	tr->trid = msgr->next_trid;
	msgr->next_trid++;
	if (msgr->next_trid == 0)
		msgr->next_trid++;
	return tr;
}

void mtran_free(struct mtran *tr)
{
	if (!IS_ERR(tr->m))
		free(tr->m);
	free(tr);
}

static int mtran_compare(struct mtran *tr_a, struct mtran *tr_b)
{
	if (tr_a->trid < tr_b->trid)
		return -1;
	else if (tr_a->trid > tr_b->trid)
		return 1;
	else
		return 0;
}

static struct mtran *mtran_lookup_by_id(struct mconn *conn, uint32_t trid)
{
	struct mtran exemplar;
	memset(&exemplar, 0, sizeof(exemplar));
	exemplar.trid = trid;
	return RB_FIND(active_tr, &conn->active_head, &exemplar);
}

void mtran_send(struct msgr *msgr, struct mtran *tr,
		uint32_t ip, uint16_t port, struct msg *m)
{
	tr->ip = ip;
	tr->port = port;
	tr->m = m;
	m->rem_trid = htobe32(tr->trid);
	m->trid = htobe32(tr->rem_trid);
	pthread_spin_lock(&msgr->lock);
	STAILQ_INSERT_TAIL(&msgr->pending_tr_head, tr, u.pending_entry);
	pthread_spin_unlock(&msgr->lock);
	ev_async_send(msgr->loop, &msgr->w_notify);
}

void mtran_send_next(struct mconn *conn, struct mtran *tr, struct msg *m)
{
	tr->m = m;
	m->rem_trid = htobe32(tr->trid);
	m->trid = htobe32(tr->rem_trid);
	STAILQ_INSERT_TAIL(&conn->pending_head, tr, u.pending_entry);
	fast_log_msgr(conn->msgr, FAST_LOG_MSGR_DEBUG,
		tr->port, tr->ip, tr->trid,
		tr->rem_trid, FLME_MTRAN_SEND_NEXT, be16toh(m->ty));
}

void mtran_recv_next(struct mconn *conn, struct mtran *tr)
{
	RB_INSERT(active_tr, &conn->active_head, tr);
}

static void mtran_deliver_netfail(struct mconn *conn,
		struct mtran *tr, int err)
{
	tr->m = ERR_PTR(FORCE_POSITIVE(err));
	conn->msgr->cb(conn, tr);
}

/****************************** mconn ********************************/
static struct mconn *mconn_create(struct msgr *msgr,
		uint32_t ip, uint16_t port, int sock)
{
	int ret;
	struct mconn *conn;
	struct sockaddr_in addr;

	if (msgr->cur_conn + 1 > msgr->max_conn) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, port, ip, 0,
			0, FLME_MAX_CONN_REACHED, cram_into_u16(msgr->max_conn));
		return ERR_PTR(ENOSPC);
	}
	conn = calloc(1, sizeof(struct mconn));
	if (!conn)
		return ERR_PTR(ENOMEM);
	ev_init(&conn->w_write, NULL);
	ev_init(&conn->w_read, NULL);
	conn->msgr = msgr;
	conn->ip = ip;
	conn->port = port;
	conn->state = MCONN_CONNECTING;
	conn->cnt = -1;
	conn->inbound_tr = NULL;
	conn->inbound_msg = NULL;
	RB_INIT(&conn->active_head);
	STAILQ_INIT(&conn->pending_head);
	RB_INSERT(msgr_conn, &msgr->conn_head, conn);
	if (sock < 0) {
		conn->sock = do_socket(AF_INET, SOCK_STREAM, 0,
				WANT_O_CLOEXEC | WANT_O_NONBLOCK);
		if (conn->sock < 0) {
			fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, port, ip, 0,
				0, FLME_DO_SOCKET_FAILED,
				cram_into_u16(FORCE_POSITIVE(conn->sock)));
			mconn_teardown(conn, -conn->sock);
			return ERR_PTR(FORCE_POSITIVE(conn->sock));
		}
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(ip);
		addr.sin_port = htons(port);
		RETRY_ON_EINTR(ret, connect(conn->sock, &addr, sizeof(addr)));
		if (ret == 0) {
			/* The connect operation succeeded immediately */
			conn->state = MCONN_QUIESCENT;
			fast_log_msgr(msgr, FAST_LOG_MSGR_DEBUG, port, ip,
					0, 0, FLME_CONN_ESTABLISHED, 1);
		}
		else {
			ret = errno;
			if (ret != EINPROGRESS) {
				fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR,
					port, ip, 0, 0,
					FLME_OUTGOING_CONN_FAILED,
					cram_into_u16(FORCE_POSITIVE(ret)));
				mconn_teardown(conn, ret);
				return ERR_PTR(FORCE_POSITIVE(ret));
			}
			/* The connect operation is in progress */
		}
	}
	else {
		conn->sock = sock;
	}
	ev_io_init(&conn->w_write, mconn_writable_cb,
		conn->sock, EV_WRITE);
	ev_io_init(&conn->w_read, mconn_readable_cb,
		conn->sock, EV_READ);
	return conn;
}

static struct mconn* mconn_find(struct msgr* msgr, uint32_t ip, uint16_t port)
{
	struct mconn exemplar;
	memset(&exemplar, 0, sizeof(exemplar));
	exemplar.ip = ip;
	exemplar.port = port;
	return RB_FIND(msgr_conn, &msgr->conn_head, &exemplar);
}

/** Tear down a connection.
 * Deliver failure messages to all pending and active transactors.
 *
 * This can be called from any state.
 */
static void mconn_teardown(struct mconn *conn, int failcode)
{
	int res, num_failed;
	struct msgr *msgr = conn->msgr;
	struct mtran *tr, *tr_tmp;

	RB_REMOVE(msgr_conn, &msgr->conn_head, conn);
	conn->msgr->cur_conn--;
	ev_io_stop(conn->msgr->loop, &conn->w_write);
	ev_io_stop(conn->msgr->loop, &conn->w_read);
	if (conn->inbound_msg) {
		free(conn->inbound_msg);
		conn->inbound_msg = NULL;
	}
	/* We don't have to check conn->inbound_tr here.  If it's non-NULL, it
	 * will just be a pointer to something in active_tr */
	if (conn->sock > 0) {
		RETRY_ON_EINTR(res, close(conn->sock));
	}
	/* Deliver a failure message to all pending transactors */
	num_failed = 0;
	while (1) {
		tr = STAILQ_FIRST(&conn->pending_head);
		if (!tr)
			break;
		STAILQ_REMOVE_HEAD(&conn->pending_head, u.pending_entry);
		mtran_deliver_netfail(conn, tr, failcode);
		++num_failed;
	}
	/* Deliver a failure message to all active transactors */
	RB_FOREACH_SAFE(tr, active_tr, &conn->active_head, tr_tmp) {
		RB_REMOVE(active_tr, &conn->active_head, tr);
		mtran_deliver_netfail(conn, tr, failcode);
		++num_failed;
	}
	if (failcode == ETIMEDOUT) {
		int severity = (num_failed == 0) ?
			FAST_LOG_MSGR_INFO : FAST_LOG_MSGR_ERROR;
		fast_log_msgr(msgr, severity, conn->port, conn->ip, 0, 0,
			FLME_CONN_TIMED_OUT, cram_into_u16(num_failed));
	}
	free(conn);
}

static void mconn_state_transition(struct mconn *conn,
				   enum mconn_state_t new_state)
{
	if (conn->state == new_state)
		return;
	fast_log_msgr(conn->msgr, FAST_LOG_MSGR_DEBUG, conn->port, conn->ip,
		0, 0, FLME_MTRAN_NEW_STATE,
		(conn->state << 4) | (new_state));
	conn->state = new_state;
}

/** Abort an incoming message
 *
 * Can be called from MCONN_QUIESCENT, MCONN_READING_HDR
 */
static void mconn_inbound_abort(struct mconn *conn)
{
	if (!((conn->state == MCONN_QUIESCENT) ||
	      (conn->state == MCONN_READING_HDR))) {
		/* illegal state */
		abort();
	}
	free(conn->inbound_msg);
	conn->inbound_msg = NULL;
	if (conn->inbound_tr != NULL)
		abort();
	conn->cnt = 0;
	mconn_state_transition(conn, MCONN_QUIESCENT);
	mconn_next_state_logic(conn);
}

static int mconn_compare(struct mconn *a, struct mconn *b)
{
	if (a->ip < b->ip)
		return -1;
	else if (a->ip > b->ip)
		return 1;
	if (a->port < b->port)
		return -1;
	else if (a->port > b->port)
		return 1;
	return 0;
}

static void mconn_next_state_logic(struct mconn *conn)
{
	enum mconn_state_t new_state = conn->state;
	struct msgr *msgr = conn->msgr;

	/* choose next state */
	switch (conn->state) {
	case MCONN_CONNECTING:
	case MCONN_AWAITING_HDR:
		break;
	case MCONN_QUIESCENT:
	case MCONN_WRITING:
		if (STAILQ_EMPTY(&conn->pending_head)) {
			new_state = MCONN_QUIESCENT;
		}
		else {
			conn->cnt = 0;
			conn->inbound_msg = NULL;
			conn->inbound_tr = NULL;
			new_state = MCONN_WRITING;
		}
		break;
	default:
		/* do nothing */
		break;
	}
	mconn_state_transition(conn, new_state);

	/* activate/deactivate io callback based on state */
	switch (conn->state) {
	case MCONN_CONNECTING:
	case MCONN_WRITING:
		ev_io_start(msgr->loop, &conn->w_write);
		ev_io_stop(msgr->loop, &conn->w_read);
		break;
	case MCONN_AWAITING_HDR:
	case MCONN_READING_HDR:
	case MCONN_READING:
	case MCONN_QUIESCENT:
		ev_io_stop(msgr->loop, &conn->w_write);
		ev_io_start(msgr->loop, &conn->w_read);
		break;
	}
}

static void mconn_writable_cb(POSSIBLY_UNUSED(struct ev_loop *loop),
		struct ev_io *w, int revents)
{
	struct mconn *conn = GET_OUTER(w, struct mconn, w_write);
	struct msgr *msgr = conn->msgr;

	if (revents & EV_ERROR) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, conn->port,
			conn->ip, 0, 0, FLME_EV_ERROR, 1);
		mconn_teardown(conn, ENOMEDIUM);
		return;
	}
	if (!(revents & EV_WRITE))
		return;
	conn->timeout_cnt = 0; /* register some activity */
	switch (conn->state) {
	case MCONN_CONNECTING: {
		int val = 0, ret;
		socklen_t val_len = sizeof(val);
		ret = getsockopt(conn->sock, SOL_SOCKET, SO_ERROR,
				&val, &val_len);
		if (ret) {
			ret = errno;
		}
		else if (val != 0) {
			ret = val;
		}
		if (ret) {
			fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR,
				conn->port, conn->ip, 0, 0,
				FLME_OUTGOING_CONN_FAILED,
				(uint16_t)ret);
			mconn_teardown(conn, ret);
			return;
		}
		fast_log_msgr(msgr, FAST_LOG_MSGR_DEBUG, conn->port, conn->ip,
			      0, 0, FLME_CONN_ESTABLISHED, 0);
		mconn_state_transition(conn, MCONN_QUIESCENT);
		mconn_next_state_logic(conn);
		return;
	}
	case MCONN_QUIESCENT:
		mconn_next_state_logic(conn);
		return;
	case MCONN_WRITING: {
		int full, amt, res;
		struct mtran *tr =
			STAILQ_FIRST(&conn->pending_head);
		if (tr == NULL) {
			fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR,
				conn->port, conn->ip, 0, 0,
				FLME_EXPECTED_PENDING_TRANSACTOR, 0);
			mconn_state_transition(conn, MCONN_QUIESCENT);
			mconn_next_state_logic(conn);
			return;
		}
		full = sizeof(struct msg) + be32toh(tr->m->len);
		amt = full - conn->cnt;
		res = write(conn->sock, tr->m, amt);
		if (res < 0) {
			int ret = errno;
			if (is_temporary_socket_error(ret))
				return;
			fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR,
				conn->port, conn->ip, tr->trid, tr->rem_trid,
				FLME_WRITE_ERROR,
				cram_into_u16(FORCE_POSITIVE(ret)));
			mconn_teardown(conn, ret);
			return;
		}
		conn->cnt += res;
		if (conn->cnt == full) {
			conn->cnt = -1;
			STAILQ_REMOVE_HEAD(&conn->pending_head, u.pending_entry);
			mconn_state_transition(conn, MCONN_QUIESCENT);
			free(tr->m);
			tr->m = NULL;
			msgr->cb(conn, tr);
			mconn_next_state_logic(conn);
		}
		return;
	}
	default:
//		glitch_log("mconn_writable_cb: internal error: unhandled "
//			   "state %d\n", conn->state);
		/* We aren't in the mood to write anything */
		mconn_next_state_logic(conn);
		return;
	}
}

static void mconn_readable_cb(POSSIBLY_UNUSED(struct ev_loop *loop),
		struct ev_io *w, int revents)
{
	int amt, res, m_len;
	struct msg *m;
	struct mconn *conn = GET_OUTER(w, struct mconn, w_read);
	struct msgr *msgr = conn->msgr;
	struct mtran *tr = NULL;
	uint32_t trid;

	if (revents & EV_ERROR) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, conn->port,
			conn->ip, 0, 0, FLME_EV_ERROR, 2);
		mconn_teardown(conn, ENOMEDIUM);
		return;
	}
	if (!(revents & EV_READ))
		return;
	conn->timeout_cnt = 0; /* register some activity */
	switch (conn->state) {
	case MCONN_QUIESCENT:
	case MCONN_AWAITING_HDR:
		mconn_state_transition(conn, MCONN_READING_HDR);
		conn->inbound_msg = calloc(1, sizeof(struct msg));
		if (!conn->inbound_msg) {
			fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, conn->port,
				conn->ip, 0, 0, FLME_OOM, 2);
			mconn_inbound_abort(conn);
			return;
		}
		conn->cnt = 0;
		mconn_state_transition(conn, MCONN_READING_HDR);
		/* fall through */
	case MCONN_READING_HDR: {
		amt = sizeof(struct msg) - conn->cnt;
		res = read(conn->sock, conn->inbound_msg, amt);
		if (res < 0) {
			int ret = errno;
			if (is_temporary_socket_error(ret))
				return;
			fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, conn->port,
				conn->ip, 0, 0, FLME_HDR_READ_ERROR, ret);
			mconn_teardown(conn, ret);
			return;
		}
		conn->cnt += res;
		if (conn->cnt != sizeof(struct msg)) {
			return;
		}
		m_len = be32toh(conn->inbound_msg->len);
		m = realloc(conn->inbound_msg, sizeof(struct msg) + m_len);
		if (!m) {
			fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, conn->port,
				conn->ip, 0, 0, FLME_OOM, 3);
			mconn_inbound_abort(conn);
			return;
		}
		conn->inbound_msg = m;
		conn->cnt = 0;
		trid = be32toh(conn->inbound_msg->trid);
		if (trid == 0) {
			/* A trid of 0 means that no transactor has been allocated
			 * yet on this side of the connection. */
			tr = mtran_alloc(msgr);
			if (!tr) {
				fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR,
					conn->port, conn->ip, 0, 0,
					FLME_OOM, 4);
				mconn_inbound_abort(conn);
				return;
			}
			tr->ip = conn->ip;
			tr->port = conn->port;
			conn->inbound_msg->trid = htobe32(tr->trid);
			tr->rem_trid = be32toh(conn->inbound_msg->rem_trid);
			RB_INSERT(active_tr, &conn->active_head, tr);
		}
		else {
			uint32_t rem_trid;
			tr = mtran_lookup_by_id(conn, trid);
			rem_trid = be32toh(conn->inbound_msg->rem_trid);
			if (!tr) {
				fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR,
					conn->port, conn->ip, trid, rem_trid,
					FLME_MTRAN_NONESUCH, 0);
				mconn_inbound_abort(conn);
				return;
			}
			if ((tr->rem_trid != 0) && (tr->rem_trid != rem_trid)) {
				fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, tr->port, tr->ip, tr->trid,
					tr->rem_trid, FLME_MTRAN_WRONG_REM_TRID, 0);
				mconn_inbound_abort(conn);
				return;
			}
		}
		conn->inbound_tr = tr;
		mconn_state_transition(conn, MCONN_READING);
		/* fall through */
	}
	case MCONN_READING:
		m_len = be32toh(conn->inbound_msg->len);
		amt = m_len - conn->cnt;
		res = read(conn->sock, conn->inbound_msg->data, amt);
		if (res < 0) {
			int ret = errno;
			if (is_temporary_socket_error(ret))
				return;
			fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, conn->port,
				conn->ip, 0, 0, FLME_READ_ERROR, ret);
			mconn_teardown(conn, ret);
			return;
		}
		conn->cnt += res;
		if (conn->cnt != m_len) {
			return;
		}
		RB_REMOVE(active_tr, &conn->active_head, conn->inbound_tr);
		mconn_state_transition(conn, MCONN_QUIESCENT);
		m = conn->inbound_msg;
		conn->inbound_msg = NULL;
		conn->cnt = -1;
		tr = conn->inbound_tr;
		conn->inbound_tr = NULL;
		tr->m = m;
		msgr->cb(conn, tr);
		mconn_next_state_logic(conn);
		return;
	default:
		/* Probably should never get here */
		mconn_next_state_logic(conn);
		return;
	}
}

/****************************** msgr ********************************/
struct msgr *msgr_init(char *err, size_t err_len,
		int max_conn, int max_tran, size_t tran_sz, msgr_cb_t cb,
		int timeout_period, int timeout_cnt_max,
		struct fast_log_mgr *fb_mgr)
{
	struct msgr *msgr;
	
	msgr = calloc(1, sizeof(struct msgr));
	if (!msgr) {
		snprintf(err, err_len, "init_msgr: out of memory\n");
		return NULL;
	}
	if (pthread_spin_init(&msgr->lock, 0)) {
		snprintf(err, err_len, "init_msgr: failed to initialize "
			"spinlock\n");
		free(msgr);
		return NULL;
	}
	msgr->state = MSGR_STATE_INIT;
	msgr->listen_fd = -1;
	msgr->tran_sz = tran_sz;
	msgr->cb = cb;
	msgr->next_trid = random();
	if (msgr->next_trid == 0)
		msgr->next_trid++;
	msgr->cur_tran = 0;
	msgr->max_tran = max_tran;
	msgr->cur_conn = 0;
	msgr->max_conn = max_conn;
	msgr->timeout_period = timeout_period;
	msgr->timeout_cnt_max = timeout_cnt_max;
	RB_INIT(&msgr->conn_head);
	ev_init(&msgr->w_listen_fd, NULL);
	STAILQ_INIT(&msgr->pending_tr_head);
	ev_async_init(&msgr->w_notify, run_msgr_notify_cb);
	ev_timer_init(&msgr->w_timeout, run_msgr_timeout_cb,
		msgr->timeout_period, msgr->timeout_period);
	msgr->loop = ev_loop_new(0);
	if (!msgr->loop) {
		snprintf(err, err_len, "init_msgr: ev_loop_new failed.");
		msgr_shutdown(msgr);
		return NULL;
	}
	ev_async_start(msgr->loop, &msgr->w_notify);
	ev_timer_start(msgr->loop, &msgr->w_timeout);
	msgr->fb_mgr = fb_mgr;
	return msgr;
}

void msgr_shutdown(struct msgr *msgr)
{
	int res, need_join = 0;
	struct mconn *conn, *conn_tmp;

	pthread_spin_lock(&msgr->lock);
	if (msgr->state == MSGR_STATE_THREAD_STARTED) {
		msgr->state = MSGR_STATE_THREAD_STOPPING;
		need_join = 1;
	}
	pthread_spin_unlock(&msgr->lock);
	if (need_join) {
		ev_async_send(msgr->loop, &msgr->w_notify);
		redfish_thread_join(&msgr->rt);
	}
	RB_FOREACH_SAFE(conn, msgr_conn, &msgr->conn_head, conn_tmp) {
		RB_REMOVE(msgr_conn, &msgr->conn_head, conn);
		mconn_teardown(conn, ESHUTDOWN);
	}
	pthread_spin_destroy(&msgr->lock);
	ev_io_stop(msgr->loop, &msgr->w_listen_fd);
	ev_async_stop(msgr->loop, &msgr->w_notify);
	ev_timer_stop(msgr->loop, &msgr->w_timeout);
	ev_loop_destroy(msgr->loop);
	if (msgr->listen_fd > 0)
		RETRY_ON_EINTR(res, close(msgr->listen_fd));
	free(msgr);
}

void msgr_listen(struct msgr *msgr, uint16_t port, char *err, size_t err_len)
{
	struct sockaddr_in addr;
	socklen_t addr_len;
	int ret, res, fd;

	if (msgr->state != MSGR_STATE_INIT) {
		snprintf(err, err_len, "msgr_listen: you must call "
			"msgr_listen before starting the messenger thread.");
		return;
	}
	if (msgr->listen_fd > 0) {
		snprintf(err, err_len, "msgr_listen: programmer error: "
			"current implementation can only listen on one port "
			"at once, but multiple were requested.");
		return;
	}
	fd = do_socket(AF_INET, SOCK_STREAM, 0,
			   WANT_O_CLOEXEC | WANT_O_NONBLOCK);
	if (fd < 0) {
		snprintf(err, err_len, "msgr_listen: failed to create socket: "
			 "error %d", fd);
		return;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	addr_len = sizeof(addr); 
	ret = bind(fd, (struct sockaddr*)&addr, addr_len);
	if (ret) {
		ret = -errno;
		snprintf(err, err_len, "msgr_listen: bind error: %d", ret);
		RETRY_ON_EINTR(res, close(fd));
		return;
	}
	ret = listen(fd, 32);
	if (ret) {
		ret = -errno;
		snprintf(err, err_len, "msgr_listen: listen error: %d", ret);
		RETRY_ON_EINTR(res, close(fd));
		return;
	}
	msgr->listen_fd = fd;
	msgr->listen_port = port;
}

static void run_msgr_timeout_cb(POSSIBLY_UNUSED(struct ev_loop *loop),
               struct ev_timer *w, int revents)
{
	struct msgr *msgr = GET_OUTER(w, struct msgr, w_timeout);
	struct mconn *conn, *conn_tmp;
	int timeout_cnt_max = msgr->timeout_cnt_max;

	if (revents & EV_ERROR) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, 0, 0, 0,
			0, FLME_EV_ERROR, 3);
		return;
	}
	RB_FOREACH_SAFE(conn, msgr_conn, &msgr->conn_head, conn_tmp) {
		conn->timeout_cnt++;
		if (conn->timeout_cnt >= timeout_cnt_max)
			mconn_teardown(conn, ETIMEDOUT);
	}
}

static void run_msgr_listen_fd_cb(POSSIBLY_UNUSED(struct ev_loop *loop),
		struct ev_io *w, int revents)
{
	int ret, fd = -1;
	struct msgr *msgr;
	struct mconn *conn;
	struct sockaddr_in remote;
	uint32_t ip;
	uint16_t port;

	msgr = GET_OUTER(w, struct msgr, w_listen_fd);
	if (revents & EV_ERROR) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, 0, 0, 0,
			0, FLME_EV_ERROR, 4);
		return;
	}
	if (!(revents & EV_READ)) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, 0, 0, 0,
			0, FLME_NO_EV_READ, 0);
		return;
	}
	fd = do_accept(msgr->listen_fd, (struct sockaddr*)&remote,
		sizeof(remote), WANT_O_CLOEXEC | WANT_O_NONBLOCK);
	if (fd < 0) {
		if (is_temporary_socket_error(-fd))
			return;
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, 0, 0, 0,
			0, FLME_ACCEPT_FAILED,
			cram_into_u16(FORCE_POSITIVE(fd)));
		goto error;
	}
	ip = ntohl(remote.sin_addr.s_addr);
	port = ntohs(remote.sin_port);
	conn = mconn_find(msgr, ip, port);
	if (conn) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, conn->port,
			      conn->ip, 0, 0, FLME_MTRAN_MULTI_CONN, 0);
		goto error;
	}
	conn = mconn_create(msgr, ip, port, fd);
	if (IS_ERR(conn)) {
		goto error;
	}
	fast_log_msgr(msgr, FAST_LOG_MSGR_DEBUG, conn->port,
		      conn->ip, 0, 0, FLME_INBOUND_CONN_CREATED, 0);
	mconn_state_transition(conn, MCONN_AWAITING_HDR);
	mconn_next_state_logic(conn);
	return;

error:
	if (fd > 0) {
		RETRY_ON_EINTR(ret, close(fd));
	}
}

static void run_msgr_setup_pending(struct msgr *msgr, struct mtran *tr)
{
	struct mconn *conn;

	conn = mconn_find(msgr, tr->ip, tr->port);
	if (!conn) {
		conn = mconn_create(msgr, tr->ip, tr->port, -1);
		if (IS_ERR(conn))
			return;
		fast_log_msgr(msgr, FAST_LOG_MSGR_DEBUG,
			tr->port, tr->ip, tr->trid, tr->rem_trid,
			FLME_OUTBOUND_CONN_CREATED, be16toh(tr->m->ty));
	}
	else {
		fast_log_msgr(msgr, FAST_LOG_MSGR_DEBUG,
			tr->port, tr->ip, tr->trid,
			tr->rem_trid, FLME_CONN_REUSED, be16toh(tr->m->ty));
	}
	STAILQ_INSERT_TAIL(&conn->pending_head, tr, u.pending_entry);
	mconn_next_state_logic(conn);
}

static void run_msgr_notify_cb(struct ev_loop *loop, struct ev_async *w,
					POSSIBLY_UNUSED(int revents))
{
	struct msgr *msgr = GET_OUTER(w, struct msgr, w_notify);
	struct mtran *tr;
	enum msgr_state_t new_state;

	//fprintf(stderr, "in run_msgr_notify_cb\n");
	while (1) {
		pthread_spin_lock(&msgr->lock);
		tr = STAILQ_FIRST(&msgr->pending_tr_head);
		if (tr) {
			STAILQ_REMOVE_HEAD(&msgr->pending_tr_head,
				u.pending_entry);
		}
		new_state = msgr->state;
		pthread_spin_unlock(&msgr->lock);

		if (new_state == MSGR_STATE_THREAD_STOPPING) {
			ev_unloop(loop, EVUNLOOP_ALL);
			return;
		}
		if (tr == NULL) {
			return;
		}
		run_msgr_setup_pending(msgr, tr);
	}
}

static int run_msgr(struct redfish_thread *rt)
{
	struct msgr *msgr = (struct msgr*)rt->init_data;

	fast_log_msgr(msgr, FAST_LOG_MSGR_INFO, 0,
		0, 0, 0, FLME_MSGR_INIT, cram_into_u16(rt->thread_id));
	if (msgr->listen_fd > 0) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_INFO, 0,
			0, 0, 0, FLME_LISTENING, msgr->listen_port);
	}
	ev_loop(msgr->loop, 0);
	fast_log_msgr(msgr, FAST_LOG_MSGR_INFO, 0,
		0, 0, 0, FLME_MSGR_SHUTDOWN, cram_into_u16(rt->thread_id));
	return 0;
}

void msgr_start(struct msgr *msgr, char *err, size_t err_len)
{
	int ret;

	if (msgr->state != MSGR_STATE_INIT) {
		snprintf(err, err_len, "msgr_start: thread has already been "
			 "started!");
		return;
	}
	if (msgr->listen_fd > 0) {
		ev_io_init(&msgr->w_listen_fd, run_msgr_listen_fd_cb,
			msgr->listen_fd, EV_WRITE | EV_READ);
		ev_io_start(msgr->loop, &msgr->w_listen_fd);
	}
	ret = redfish_thread_create(msgr->fb_mgr, &msgr->rt,
				run_msgr, msgr, NULL);
	if (ret) {
		snprintf(err, err_len, "msgr_start: pthread_create failed "
			 "with error %d", ret);
		return;
	}
	msgr->state = MSGR_STATE_THREAD_STARTED;
}
