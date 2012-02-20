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

#include "msg/fast_log.h"
#include "msg/generic.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "util/circ_compare.h"
#include "util/cram.h"
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
static void mtran_deliver_netfail(struct mtran *tr, int err);
static int mtran_compare_trid(struct mtran *a, struct mtran *b) PURE;
static int mtran_compare_timeo(struct mtran *a, struct mtran *b) PURE;

/****************************** types ********************************/
enum mconn_state_t {
	MCONN_CONNECTING = 0,
	MCONN_ESTABLISHED,
};

enum {
	MSGR_RET_CONTINUE = 0,
	MSGR_RET_STOP = 1,
};

STAILQ_HEAD(pending_tr, mtran);
RB_HEAD(active_tr, mtran);
RB_GENERATE(active_tr, mtran, u.active_entry, mtran_compare_trid);
RB_HEAD(timeo_tr, mtran);
RB_GENERATE(timeo_tr, mtran, timeo_entry, mtran_compare_timeo);

struct conn_cancel {
	SLIST_ENTRY(conn_cancel) entry;
	uint32_t addr;
	uint16_t port;
};

SLIST_HEAD(conn_cancels, conn_cancel);

struct mlisten {
	/** Callback to invoke on sent/recv */
	msgr_cb_t cb;
	/** Private data to store in transactor */
	void *priv;
	/** File descriptor */
	int fd;
	/** TCP port number */
	uint16_t port;
};

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
	/** number of bytes sent */
	int sent_cnt;
	/** number of bytes received */
	int recv_cnt;
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
	/** All transactions. Keyed on transactor timeout. */
	struct timeo_tr timeo_head;
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
	/** lock that protects thread state, pending_tr_head,
	 * conn_cancels_head, and (sort of) timeo_id */
	pthread_spinlock_t lock;
	/** messenger thread state */
	enum msgr_state_t state;
	/** thread */
	struct redfish_thread rt;
	/** Main loop */
	struct ev_loop *loop;
	/** Listener */
	struct mlisten listen;
	/** Watches listen_fd */
	struct ev_io w_listen_fd;
	/** connections to cancel */
	struct conn_cancels conn_cancels_head;
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
	/** Async watcher. Lets us know that another thread asked us to shut
	 * down or send a message. */
	struct ev_async w_notify;
	/** event watcher for connection timeout */
	struct ev_timer w_timeout;
	/** Pending transactions not yet assigned to a connection */
	struct pending_tr pending_tr_head;
	/** Fast log buffer manager */
	struct fast_log_mgr *fb_mgr;
	/** Maximum number of timeout periods to wait for before timing out a
	 * connection or transactor */
	int tcp_teardown_timeo;
	/** Current timeout period ID. */
	uint16_t timeo_id;
	/** The name of this messenger */
	const char *name;
};

/****************************** utility ********************************/
static int is_temporary_socket_error(int err)
{
	return ((err == EAGAIN) || (err == EWOULDBLOCK) || (err == EINTR));
}

void fast_log_msgr(struct msgr *msgr, uint16_t ty,
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
	struct mtran *tr = calloc(1, sizeof(struct mtran));
	if (!tr)
		return NULL;
	tr->trid = msgr->next_trid;
	msgr->next_trid++;
	if (msgr->next_trid == 0)
		msgr->next_trid++;
	tr->state = MTRAN_STATE_IDLE;
	return tr;
}

void mtran_free(struct mtran *tr)
{
	if (!IS_ERR(tr->m))
		free(tr->m);
	free(tr);
}

static int mtran_compare_trid(struct mtran *a, struct mtran *b)
{
	if (a->trid < b->trid)
		return -1;
	else if (a->trid > b->trid)
		return 1;
	else
		return 0;
}

static int mtran_compare_timeo(struct mtran *a, struct mtran *b)
{
	/* Do a circular comparison of the timeout period.  The correctness of
	 * this function is based on the fact that we'll never have a transactor
	 * lingering for 546 hours (2**15 seconds)  */
	return circ_compare16(a->timeo_id, b->timeo_id);
}

static struct mtran *mtran_lookup_by_id(struct mconn *conn, uint32_t trid)
{
	struct mtran exemplar;
	memset(&exemplar, 0, sizeof(exemplar));
	exemplar.trid = trid;
	return RB_FIND(active_tr, &conn->active_head, &exemplar);
}

void mtran_send(struct msgr *msgr, struct mtran *tr,
		msgr_cb_t cb, void *priv, struct msg *m, int timeo)
{
	if (timeo > MSGR_TIMEOUT_MAX) {
		mtran_deliver_netfail(tr, EINVAL);
		return;
	}
	tr->state = MTRAN_STATE_SENDING;
	tr->cb = cb;
	tr->priv = priv;
	tr->m = m;
	m->rem_trid = htobe32(tr->trid);
	m->trid = htobe32(tr->rem_trid);
	pthread_spin_lock(&msgr->lock);
	if (msgr->state == MSGR_STATE_THREAD_STOPPING) {
		/* Once the messenger is in shutdown, we don't want to add any
		 * new transactors to the pending queue.
		 */
		pthread_spin_unlock(&msgr->lock);
		mtran_deliver_netfail(tr, ECANCELED);
		return;
	}
	tr->timeo_id = (uint16_t)msgr->timeo_id + (uint16_t)timeo;
	/* Add our transactor to the pending queue and poke the messenger
	 * thread.  It will decide which connection (mconn) to give the
	 * transactor to. */
	STAILQ_INSERT_TAIL(&msgr->pending_tr_head, tr, u.pending_entry);
	pthread_spin_unlock(&msgr->lock);
	ev_async_send(msgr->loop, &msgr->w_notify);
}

void mtran_send_next(struct mconn *conn, struct mtran *tr, struct msg *m,
		int timeo)
{
	if (timeo > MSGR_TIMEOUT_MAX) {
		mtran_deliver_netfail(tr, EINVAL);
		return;
	}
	/* There is no need for any locks in this function.  mtran_send_next can
	 * only be invoked from the context of a callback made by the messenger
	 * thread itself.  Since all modifications to struct mconn are made from
	 * that same messenger thread, there is no concurrency hazard. */
	tr->state = MTRAN_STATE_SENDING;
	tr->m = m;
	tr->timeo_id = (uint16_t)conn->msgr->timeo_id + (uint16_t)timeo;
	m->rem_trid = htobe32(tr->trid);
	m->trid = htobe32(tr->rem_trid);
	STAILQ_INSERT_TAIL(&conn->pending_head, tr, u.pending_entry);
	RB_INSERT(timeo_tr, &conn->timeo_head, tr);
	fast_log_msgr(conn->msgr, FAST_LOG_MSGR_DEBUG,
		tr->port, tr->ip, tr->trid,
		tr->rem_trid, FLME_MTRAN_SEND_NEXT, be16toh(m->ty));
	ev_io_start(conn->msgr->loop, &conn->w_write);
}

void mtran_recv_next(struct mconn *conn, struct mtran *tr)
{
	tr->state = MTRAN_STATE_ACTIVE;
	RB_INSERT(active_tr, &conn->active_head, tr);
	RB_INSERT(timeo_tr, &conn->timeo_head, tr);
}

int mconn_cancel(struct msgr *msgr, uint32_t addr, uint16_t port)
{
	struct conn_cancel *cancel;

	cancel = calloc(1, sizeof(struct conn_cancel));
	if (!cancel)
		return -ENOMEM;
	cancel->addr = addr;
	cancel->port = port;
	pthread_spin_lock(&msgr->lock);
	if (msgr->state == MSGR_STATE_THREAD_STOPPING) {
		pthread_spin_unlock(&msgr->lock);
		return -ECANCELED;
	}
	SLIST_INSERT_HEAD(&msgr->conn_cancels_head, cancel, entry);
	pthread_spin_unlock(&msgr->lock);
	ev_async_send(msgr->loop, &msgr->w_notify);
	return 0;
}

static void mtran_deliver_netfail(struct mtran *tr, int err)
{
	if (tr->state == MTRAN_STATE_SENDING)
		tr->state = MTRAN_STATE_SENT;
	else
		tr->state = MTRAN_STATE_RECV;
	if (!IS_ERR(tr->m))
		free(tr->m);
	tr->m = ERR_PTR(FORCE_POSITIVE(err));
	tr->cb(NULL, tr);
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
	conn->sent_cnt = 0;
	conn->recv_cnt = 0;
	conn->inbound_tr = NULL;
	conn->inbound_msg = NULL;
	RB_INIT(&conn->active_head);
	RB_INIT(&conn->timeo_head);
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
			conn->state = MCONN_ESTABLISHED;
			fast_log_msgr(msgr, FAST_LOG_MSGR_DEBUG, port, ip,
					0, 0, FLME_CONN_ESTABLISHED, 1);
		}
		else {
			ret = errno;
			conn->state = MCONN_CONNECTING;
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
		ev_io_init(&conn->w_write, mconn_writable_cb,
			conn->sock, EV_WRITE);
		ev_io_start(msgr->loop, &conn->w_write);
	}
	else {
		conn->sock = sock;
		conn->state = MCONN_ESTABLISHED;
		ev_io_init(&conn->w_write, mconn_writable_cb,
			conn->sock, EV_WRITE);
	}
	ev_io_init(&conn->w_read, mconn_readable_cb,
		conn->sock, EV_READ);
	ev_io_start(msgr->loop, &conn->w_read);
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

	/* NOTE: we don't bother removing transactors from the timeout tree
	 * here.  There isn't any reason to do it. */
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
		mtran_deliver_netfail(tr, failcode);
		++num_failed;
	}
	/* Deliver a failure message to all active transactors */
	RB_FOREACH_SAFE(tr, active_tr, &conn->active_head, tr_tmp) {
		RB_REMOVE(active_tr, &conn->active_head, tr);
		mtran_deliver_netfail(tr, failcode);
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

static void mconn_handle_connect(struct msgr *msgr, struct mconn *conn)
{
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
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, conn->port,
			conn->ip, 0, 0, FLME_OUTGOING_CONN_FAILED,
			cram_into_u16(ret));
		mconn_teardown(conn, ret);
		return;
	}
	fast_log_msgr(msgr, FAST_LOG_MSGR_DEBUG, conn->port, conn->ip,
		      0, 0, FLME_CONN_ESTABLISHED, 0);
	conn->state = MCONN_ESTABLISHED;
}

static void mconn_writable_cb(POSSIBLY_UNUSED(struct ev_loop *loop),
		struct ev_io *w, int revents)
{
	int ret;
	int full, amt, res;
	struct mconn *conn = GET_OUTER(w, struct mconn, w_write);
	struct msgr *msgr = conn->msgr;
	struct mtran *tr;

	if (revents & EV_ERROR) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, conn->port,
			conn->ip, 0, 0, FLME_EV_ERROR, 1);
		mconn_teardown(conn, ENOMEDIUM);
		return;
	}
	if (!(revents & EV_WRITE))
		return;
	conn->timeout_cnt = 0; /* register some activity */
	if (conn->state == MCONN_CONNECTING) {
		mconn_handle_connect(msgr, conn);
		return;
	}
	/* let's send some data */
	tr = STAILQ_FIRST(&conn->pending_head);
	if (!tr) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR,
			conn->port, conn->ip, 0, 0,
			FLME_EXPECTED_PENDING_TRANSACTOR, 0);
		ev_io_stop(msgr->loop, &conn->w_write);
		return;
	}
	if (tr->state != MTRAN_STATE_SENDING)
		abort();
	full = be32toh(tr->m->len);
	amt = full - conn->sent_cnt;
	if (amt <= 0)
		abort();
	res = send(conn->sock, tr->m, amt, 0);
	if (res < 0) {
		ret = errno;
		if (is_temporary_socket_error(ret))
			return;
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR,
			conn->port, conn->ip, tr->trid, tr->rem_trid,
			FLME_WRITE_ERROR,
			cram_into_u16(FORCE_POSITIVE(ret)));
		mconn_teardown(conn, ret);
		return;
	}
	conn->sent_cnt += res;
	if (conn->sent_cnt != full)
		return;
	conn->sent_cnt = 0;
	STAILQ_REMOVE_HEAD(&conn->pending_head, u.pending_entry);
	RB_REMOVE(timeo_tr, &conn->timeo_head, tr);
	if (!STAILQ_FIRST(&conn->pending_head))
		ev_io_stop(msgr->loop, &conn->w_write);
	free(tr->m);
	tr->m = NULL;
	tr->state = MTRAN_STATE_SENT;
	tr->cb(conn, tr);
}

static struct mtran* mconn_create_mtran(struct msgr *msgr, struct mconn *conn,
					msgr_cb_t cb)
{
	struct mtran *tr;

	tr = mtran_alloc(msgr);
	if (!tr) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR,
			conn->port, conn->ip, 0, 0,
			FLME_OOM, 4);
		mconn_teardown(conn, ENOMEM);
		return ERR_PTR(MSGR_RET_STOP);
	}
	tr->ip = conn->ip;
	tr->port = conn->port;
	conn->inbound_msg->trid = htobe32(tr->trid);
	tr->rem_trid = be32toh(conn->inbound_msg->rem_trid);
	tr->cb = cb;
	tr->priv = msgr->listen.priv;
	tr->state = MTRAN_STATE_ACTIVE;
	tr->timeo_id = (uint16_t)msgr->timeo_id +
		(uint16_t)MSGR_INCOMING_TIMEO;
	RB_INSERT(active_tr, &conn->active_head, tr);
	return tr;
}

static void mtran_handle_orphan(POSSIBLY_UNUSED(struct mconn *conn),
				struct mtran *tr)
{
	/* We ignore messages that were sent to an invalid transactor ID.
	 * We already issued an error fast_log about the event in
	 * mconn_read_msg_hdr, so there's nothing to do here.
	 *
	 * The only reason we even bother to read these messages at all is to
	 * get the data out of the socket, so that more possibly good data can
	 * come through.  I considered closing the whole socket when a message
	 * was sent to an invalid transactor ID, but that seemed too harsh.
	 */
	mtran_free(tr);
}

static int mconn_read_msg_hdr(struct msgr *msgr, struct mconn *conn)
{
	struct mtran *tr;
	struct msg *m;
	int amt, res, ret;
	uint32_t m_len, trid, rem_trid;

	/* The message header tells us how long the complete message will be */
	fast_log_msgr(msgr, FAST_LOG_MSGR_DEBUG, conn->port,
		conn->ip, 0, 0, FLME_READING_MSG_HEADER,
		cram_into_u16(conn->recv_cnt));
	if (!conn->inbound_msg) {
		conn->inbound_msg = calloc(1, sizeof(struct msg));
		if (!conn->inbound_msg) {
			fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR,
				conn->port, conn->ip, 0, 0, FLME_OOM, 2);
			mconn_teardown(conn, ENOMEM);
			return MSGR_RET_STOP;
		}
	}
	amt = sizeof(struct msg) - conn->recv_cnt;
	res = read(conn->sock, conn->inbound_msg, amt);
	if (res < 0) {
		ret = errno;
		if (is_temporary_socket_error(ret))
			return MSGR_RET_STOP;
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, conn->port,
			conn->ip, 0, 0, FLME_HDR_READ_ERROR, ret);
		mconn_teardown(conn, ret);
		return MSGR_RET_STOP;
	}
	conn->recv_cnt += res;
	if (conn->recv_cnt < (int)sizeof(struct msg)) {
		return MSGR_RET_STOP;
	}
	m_len = be32toh(conn->inbound_msg->len);
	if (m_len < sizeof(struct msg)) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, conn->port,
			conn->ip, 0, 0, FLME_HDR_READ_ERROR, ENODATA);
		mconn_teardown(conn, ENAMETOOLONG);
		return MSGR_RET_STOP;
	}
	m = realloc(conn->inbound_msg, m_len);
	if (!m) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, conn->port,
			conn->ip, 0, 0, FLME_OOM, 3);
		mconn_teardown(conn, ENOMEM);
		return MSGR_RET_STOP;
	}
	conn->inbound_msg = m;
	trid = be32toh(conn->inbound_msg->trid);
	if (trid == 0) {
		/* A trid of 0 means that no transactor has been allocated yet
		 * on this side of the connection. */
		tr = mconn_create_mtran(msgr, conn, msgr->listen.cb);
		if (IS_ERR(tr))
			return PTR_ERR(tr);
	}
	else {
		tr = mtran_lookup_by_id(conn, trid);
		rem_trid = be32toh(conn->inbound_msg->rem_trid);
		if (!tr) {
			fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR,
				conn->port, conn->ip, trid, rem_trid,
				FLME_MTRAN_NONESUCH, 0);
			tr = mconn_create_mtran(msgr, conn, mtran_handle_orphan);
			if (IS_ERR(tr))
				return PTR_ERR(tr);
		}
		if ((tr->rem_trid != 0) && (tr->rem_trid != rem_trid)) {
			fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, tr->port, tr->ip, tr->trid,
				tr->rem_trid, FLME_MTRAN_WRONG_REM_TRID, 0);
			tr = mconn_create_mtran(msgr, conn, mtran_handle_orphan);
			if (IS_ERR(tr))
				return PTR_ERR(tr);
		}
	}
	if (tr->state != MTRAN_STATE_ACTIVE)
		abort();
	conn->inbound_tr = tr;
	return MSGR_RET_CONTINUE;
}

static void mconn_readable_cb(POSSIBLY_UNUSED(struct ev_loop *loop),
		struct ev_io *w, int revents)
{
	int amt, res;
	struct mconn *conn = GET_OUTER(w, struct mconn, w_read);
	struct msgr *msgr = conn->msgr;
	struct mtran *tr;
	uint32_t m_len;

	if (revents & EV_ERROR) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, conn->port,
			conn->ip, 0, 0, FLME_EV_ERROR, 2);
		mconn_teardown(conn, ENOMEDIUM);
		return;
	}
	if (!(revents & EV_READ))
		return;
	conn->timeout_cnt = 0; /* register some activity */
	if (conn->recv_cnt < (int)sizeof(struct msg)) {
		if (mconn_read_msg_hdr(msgr, conn) != MSGR_RET_CONTINUE)
			return;
	}
	/* read the remainder of the message body */
	m_len = be32toh(conn->inbound_msg->len);
	amt = m_len - conn->recv_cnt;
	if (amt > 0) {
		res = recv(conn->sock, conn->inbound_msg->data, amt, 0);
		if (res <= 0) {
			int ret = errno;
			if (is_temporary_socket_error(ret))
				return;
			fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR,
				conn->port, conn->ip, 0, 0,
				FLME_READ_ERROR, ret);
			mconn_teardown(conn, ret);
			return;
		}
		conn->recv_cnt += res;
		if (conn->recv_cnt != (int)m_len) {
			return;
		}
	}
	/* deliver the message */
	tr = conn->inbound_tr;
	conn->inbound_tr = NULL;
	RB_REMOVE(active_tr, &conn->active_head, tr);
	RB_REMOVE(timeo_tr, &conn->timeo_head, tr);
	conn->recv_cnt = 0;
	tr->m = conn->inbound_msg;
	conn->inbound_msg = NULL;
	tr->state = MTRAN_STATE_RECV;
	tr->cb(conn, tr);
}

/****************************** msgr ********************************/
struct msgr *msgr_init(char *err, size_t err_len,
		int max_conn, int max_tran, int tcp_teardown_timeo,
		struct fast_log_mgr *fb_mgr, const char *name)
{
	struct msgr *msgr;

	msgr = calloc(1, sizeof(struct msgr));
	if (!msgr) {
		snprintf(err, err_len, "msgr_init: out of memory\n");
		return NULL;
	}
	if (pthread_spin_init(&msgr->lock, 0)) {
		snprintf(err, err_len, "msgr_init: failed to initialize "
			"spinlock\n");
		free(msgr);
		return NULL;
	}
	msgr->name = name;
	msgr->state = MSGR_STATE_INIT;
	msgr->listen.fd = -1;
	msgr->next_trid = random();
	if (msgr->next_trid == 0)
		msgr->next_trid++;
	msgr->cur_tran = 0;
	msgr->max_tran = max_tran;
	msgr->cur_conn = 0;
	msgr->max_conn = max_conn;
	msgr->tcp_teardown_timeo = tcp_teardown_timeo;
	RB_INIT(&msgr->conn_head);
	ev_init(&msgr->w_listen_fd, NULL);
	STAILQ_INIT(&msgr->pending_tr_head);
	ev_async_init(&msgr->w_notify, run_msgr_notify_cb);
	ev_timer_init(&msgr->w_timeout, run_msgr_timeout_cb,
			MSGR_TIMEOUT_PERIOD, MSGR_TIMEOUT_PERIOD);
	msgr->loop = ev_loop_new(0);
	if (!msgr->loop) {
		snprintf(err, err_len, "msgr_init: ev_loop_new failed.");
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
	int need_join = 0;
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
		mconn_teardown(conn, ECANCELED);
	}
}

void msgr_free(struct msgr *msgr)
{
	int res;

	pthread_spin_destroy(&msgr->lock);
	ev_io_stop(msgr->loop, &msgr->w_listen_fd);
	ev_async_stop(msgr->loop, &msgr->w_notify);
	ev_timer_stop(msgr->loop, &msgr->w_timeout);
	ev_loop_destroy(msgr->loop);
	if (msgr->listen.fd > 0)
		RETRY_ON_EINTR(res, close(msgr->listen.fd));
	free(msgr);
}

void msgr_listen(struct msgr *msgr, const struct listen_info *linfo,
		char *err, size_t err_len)
{
	struct sockaddr_in addr;
	socklen_t addr_len;
	int ret, res, fd;

	if (msgr->state != MSGR_STATE_INIT) {
		snprintf(err, err_len, "msgr_listen: you must call "
			"msgr_listen before starting the messenger thread.");
		return;
	}
	if (msgr->listen.fd > 0) {
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
	addr.sin_port = htons(linfo->port);
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
	msgr->listen.fd = fd;
	msgr->listen.port = linfo->port;
	msgr->listen.cb = linfo->cb;
	msgr->listen.priv = linfo->priv;
}

static void run_msgr_timeout_cb(POSSIBLY_UNUSED(struct ev_loop *loop),
               struct ev_timer *w, int revents)
{
	struct msgr *msgr = GET_OUTER(w, struct msgr, w_timeout);
	struct mconn *conn, *conn_tmp;
	struct mtran *tr, *tr_tmp;
	int tcp_teardown_timeo = msgr->tcp_teardown_timeo;
	uint16_t timeo_id;

	pthread_spin_lock(&msgr->lock);
	/* Locking rules for timeo_id:
	 *              MSGR THREAD             OTHER THREADS
	 * READ:        no locking              need msgr_lock
	 * WRITE:       need msgr_lock          don't!
	 */
	timeo_id = ++msgr->timeo_id;
	pthread_spin_unlock(&msgr->lock);
	if (revents & EV_ERROR) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_ERROR, 0, 0, 0,
			0, FLME_EV_ERROR, 3);
		return;
	}
	RB_FOREACH_SAFE(conn, msgr_conn, &msgr->conn_head, conn_tmp) {
		conn->timeout_cnt++;
		if (conn->timeout_cnt >= tcp_teardown_timeo) {
			/* Tear down the whole TCP connection because it's been
			 * inactive for too long. */
			mconn_teardown(conn, ETIMEDOUT);
			continue;
		}
		RB_FOREACH_SAFE(tr, timeo_tr, &conn->timeo_head, tr_tmp) {
			if (circ_compare16(tr->timeo_id, timeo_id) < 0) {
				/* This transactor is not yet ready to be timed
				 * out.  Since the tree is sorted, this means we
				 * can stop looking for transactors to time out.
				 * */
				break;
			}
			/* Time out transactor */
			RB_REMOVE(timeo_tr, &conn->timeo_head, tr);
			if (!(RB_REMOVE(active_tr, &conn->active_head, tr))) {
				STAILQ_REMOVE(&conn->pending_head, tr, mtran,
					u.pending_entry);
			}
			mtran_deliver_netfail(tr, ETIMEDOUT);
		}
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
	fd = do_accept(msgr->listen.fd, (struct sockaddr*)&remote,
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
		ev_io_start(msgr->loop, &conn->w_write);
	}
	RB_INSERT(timeo_tr, &conn->timeo_head, tr);
	STAILQ_INSERT_TAIL(&conn->pending_head, tr, u.pending_entry);
}

static void msgr_cancel_all_pending_tr(struct msgr *msgr)
{
	struct mtran *tr;

	while (1) {
		tr = STAILQ_FIRST(&msgr->pending_tr_head);
		if (!tr)
			break;
		STAILQ_REMOVE_HEAD(&msgr->pending_tr_head,
				u.pending_entry);
		mtran_deliver_netfail(tr, ECANCELED);
	}
}

static void run_msgr_notify_cb(struct ev_loop *loop, struct ev_async *w,
					POSSIBLY_UNUSED(int revents))
{
	struct msgr *msgr = GET_OUTER(w, struct msgr, w_notify);
	struct mtran *tr;
	enum msgr_state_t new_state;
	struct conn_cancels conn_cancels_head =
		SLIST_HEAD_INITIALIZER(conn_cancels_head);
	struct conn_cancel *cancel;
	struct mconn exemplar, *conn;

	while (1) {
		pthread_spin_lock(&msgr->lock);
		tr = STAILQ_FIRST(&msgr->pending_tr_head);
		if (tr) {
			STAILQ_REMOVE_HEAD(&msgr->pending_tr_head,
				u.pending_entry);
		}
		new_state = msgr->state;
		SLIST_SWAP(&msgr->conn_cancels_head, &conn_cancels_head,
			conn_cancel);
		pthread_spin_unlock(&msgr->lock);

		if (new_state == MSGR_STATE_THREAD_STOPPING) {
			/* Free all pending cancellations.  They're irrelevant
			 * now because soon everything will be cancelled. */
			while (1) {
				cancel = SLIST_FIRST(&conn_cancels_head);
				if (!cancel)
					break;
				SLIST_REMOVE_HEAD(&conn_cancels_head, entry);
				free(cancel);
			}
			/* We don't need to take the msgr->lock here.  The other
			 * place where msgr->pending_tr_head could be modified,
			 * in mtran_send, will never touch the queue when we're
			 * in state MSGR_STATE_THREAD_STOPPING.
			 */
			if (tr) {
				STAILQ_INSERT_TAIL(&msgr->pending_tr_head,
						tr, u.pending_entry);
			}
			msgr_cancel_all_pending_tr(msgr);
			ev_unloop(loop, EVUNLOOP_ALL);
			return;
		}
		/* Execute all pending cancellations. */
		while (1) {
			cancel = SLIST_FIRST(&conn_cancels_head);
			if (!cancel)
				break;
			memset(&exemplar, 0, sizeof(exemplar));
			exemplar.ip = cancel->addr;
			exemplar.port = cancel->port;
			conn = RB_FIND(msgr_conn, &msgr->conn_head, &exemplar);
			if (!conn) {
				continue;
			}
			mconn_teardown(conn, ECANCELED);
			free(cancel);
			SLIST_REMOVE_HEAD(&conn_cancels_head, entry);
		}
		if (!tr)
			return;
		run_msgr_setup_pending(msgr, tr);
	}
}

static int run_msgr(struct redfish_thread *rt)
{
	struct msgr *msgr = (struct msgr*)rt->priv;

	/* Name the fast log buffer for our messenger thread */
	fast_log_set_name(rt->fb, msgr->name);

	fast_log_msgr(msgr, FAST_LOG_MSGR_INFO, 0,
		0, 0, 0, FLME_MSGR_INIT, cram_into_u16(rt->thread_id));
	if (msgr->listen.fd > 0) {
		fast_log_msgr(msgr, FAST_LOG_MSGR_INFO, 0,
			0, 0, 0, FLME_LISTENING, msgr->listen.port);
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
	if (msgr->listen.fd > 0) {
		ev_io_init(&msgr->w_listen_fd, run_msgr_listen_fd_cb,
			msgr->listen.fd, EV_WRITE | EV_READ);
		ev_io_start(msgr->loop, &msgr->w_listen_fd);
	}
	ret = redfish_thread_create(msgr->fb_mgr, &msgr->rt,
				run_msgr, msgr);
	if (ret) {
		snprintf(err, err_len, "msgr_start: pthread_create failed "
			 "with error %d", ret);
		return;
	}
	msgr->state = MSGR_STATE_THREAD_STARTED;
}
