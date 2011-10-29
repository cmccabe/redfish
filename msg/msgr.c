/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/glitch_log.h"
#include "msg/generic.h"
#include "msg/msg.h"
#include "msg/msgr.h"
#include "util/error.h"
#include "util/macro.h"
#include "util/platform/socket.h"
#include "util/queue.h"
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
static void run_msgr_notify_cb(struct ev_loop *loop, struct ev_async *w,
		int revents);
static void mconn_next_state_logic(struct mconn *conn);

/****************************** types ********************************/
enum mconn_state_t {
	MCONN_CONNECTING = 0,
	MCONN_QUIESCENT,
	MCONN_WRITING,
	MCONN_READING_HDR,
	MCONN_READING,
};

STAILQ_HEAD(pending_tr, mtran);
RB_HEAD(active_tr, mtran);
RB_GENERATE(active_tr, mtran, u.active_entry, mtran_compare);

struct mconn {
	RB_ENTRY(mconn) entry;
	/** The messenger this connection is associated with */
	struct msgr *msgr;
	/** remote IP address */
	uint32_t remote_ip;
	/** remote port */
	uint16_t remote_port;
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
	/** pthread */
	pthread_t thread;
	/** Main loop */
	struct ev_loop *loop;
	/** Socket we're listening on, or -1 if we're not listening on any
	 * sockets */
	int listen_fd;
	/** Size to use when allocating mtran objects on the heap */ 
	size_t tran_sz;
	/** Callback to invoke when transactions receive messages */
	msgr_cb_t cb;
	/** Next transaction ID that will be given out */
	uint32_t next_tran_id;
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
	/** Pending transactions not yet assigned to a connection */
	struct pending_tr pending_head;
};

/****************************** utility ********************************/
static int is_temporary_socket_error(int err)
{
	return ((err == EAGAIN) || (err == EWOULDBLOCK) || (err == EINTR));
}

static void ipv4_to_str(uint32_t addr, char *out, size_t out_len)
{
	addr = htonl(addr);
	inet_ntop(AF_INET, &addr, out, out_len);
}

/****************************** mtran ********************************/
void *mtran_alloc(struct msgr *msgr)
{
	struct mtran *tr = calloc(1, msgr->tran_sz);
	if (!tr)
		return NULL;
	tr->id = msgr->next_tran_id;
	msgr->next_tran_id++;
	return tr;
}

void mtran_free(void *tr)
{
	free(tr);
}

static int mtran_compare(struct mtran *tr_a, struct mtran *tr_b)
{
	if (tr_a->id < tr_b->id)
		return -1;
	else if (tr_a->id > tr_b->id)
		return 1;
	else
		return 0;
}

static struct mtran *mtran_lookup_by_id(struct mconn *conn, uint32_t id)
{
	struct mtran exemplar;
	memset(&exemplar, 0, sizeof(exemplar));
	exemplar.id = id;
	return RB_FIND(active_tr, &conn->active_head, &exemplar);
}

void mtran_send(struct msgr *msgr, struct mtran *tr,
		uint32_t ip, uint16_t port, struct msg *m)
{
	tr->ip = ip;
	tr->port = port;
	tr->m = m;
	pthread_spin_lock(&msgr->lock);
	STAILQ_INSERT_TAIL(&msgr->pending_head, tr, u.pending_entry);
	pthread_spin_unlock(&msgr->lock);
	ev_async_send(msgr->loop, &msgr->w_notify);
}

void mtran_send_next(struct mconn *conn, struct mtran *tr, struct msg *m)
{
	tr->m = m;
	STAILQ_INSERT_TAIL(&conn->pending_head, tr, u.pending_entry);
}

void mtran_recv_next(struct mconn *conn, struct mtran *tr)
{
	RB_INSERT(active_tr, &conn->active_head, tr);
}

static void mtran_deliver_netfail(struct mconn *conn,
		struct mtran *tr, int32_t err)
{
	struct mmm_netfail *m;

	m = calloc_msg(MMM_NETFAIL, sizeof(struct mmm_netfail));
	if (!m) {
		glitch_log("mtran_deliver_netfail: OOM while "
			   "reporting network error\n");
		return;
	}
	m->error = htobe32(err);
	conn->msgr->cb(conn, tr, (struct msg*)m);
}

/****************************** mconn ********************************/
static struct mconn *mconn_create(struct msgr *msgr,
		uint32_t ip, uint16_t port, int sock)
{
	int ret;
	struct mconn *conn;
	struct sockaddr_in addr;
	char addr_str[INET_ADDRSTRLEN];

	ipv4_to_str(ip, addr_str, sizeof(addr_str));
	if (msgr->cur_conn + 1 > msgr->max_conn) {
		glitch_log("%p: mconn_create: already at max_conn "
			"(%d); can't add another connection\n",
			msgr, msgr->max_conn);
		return ERR_PTR(ENOSPC);
	}
	conn = calloc(1, sizeof(struct mconn));
	if (!conn)
		return ERR_PTR(ENOMEM);
	ev_init(&conn->w_write, NULL);
	ev_init(&conn->w_read, NULL);
	conn->msgr = msgr;
	conn->remote_ip = ip;
	conn->remote_port = port;
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
			glitch_log("%p: do_socket failed with "
				"error %d\n", msgr, conn->sock);
			mconn_teardown(conn, -conn->sock);
			return ERR_PTR(FORCE_POSITIVE(conn->sock));
		}
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(ip);
		addr.sin_port = htons(port);
		ret = connect(conn->sock, &addr, sizeof(addr));
		if (ret == 0) {
			/* The connect operation succeeded immediately */
			conn->state = MCONN_QUIESCENT;
			glitch_log("%p: mconn_create: immediately connected "
				   "to (%s:%d)\n", msgr, addr_str, port);
		}
		else {
			ret = errno;
			if (ret != EINPROGRESS) {
				glitch_log("%p: mconn_create: error connecting "
					   "to (%s:%d): error %d\n",
					   msgr, addr_str, port, ret);
				mconn_teardown(conn, ret);
				return ERR_PTR(FORCE_POSITIVE(ret));
			}
			glitch_log("%p: mconn_create: connecting "
				   "to (%s:%d)\n", msgr, addr_str, port);
			/* The connect operation is in progress */
		}
	}
	else {
		glitch_log("%p: mconn_create: accepted connection from "
			   "(%s:%d)\n", msgr, addr_str, port);
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
	exemplar.remote_ip = ip;
	exemplar.remote_port = port;
	return RB_FIND(msgr_conn, &msgr->conn_head, &exemplar);
}

/** Tear down a connection.
 * Deliver failure messages to all pending and active transactors.
 *
 * This can be called from any state.
 */
static void mconn_teardown(struct mconn *conn, int failcode)
{
	int res;
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
	while (1) {
		tr = STAILQ_FIRST(&conn->pending_head);
		if (!tr)
			break;
		STAILQ_REMOVE_HEAD(&conn->pending_head, u.pending_entry);
		mtran_deliver_netfail(conn, tr, failcode);
	}
	/* Deliver a failure message to all active transactors */
	RB_FOREACH_SAFE(tr, active_tr, &conn->active_head, tr_tmp) {
		RB_REMOVE(active_tr, &conn->active_head, tr);
		mtran_deliver_netfail(conn, tr, failcode);
	}
	free(conn);
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
	conn->state = MCONN_QUIESCENT;
	mconn_next_state_logic(conn);
}

static int mconn_compare(struct mconn *a, struct mconn *b)
{
	if (a->remote_ip < b->remote_ip)
		return -1;
	else if (a->remote_ip > b->remote_ip)
		return 1;
	if (a->remote_port < b->remote_port)
		return -1;
	else if (a->remote_port > b->remote_port)
		return 1;
	return 0;
}

static void mconn_next_state_logic(struct mconn *conn)
{
	enum mconn_state_t old_state = conn->state;

	/* choose next state */
	switch (conn->state) {
	case MCONN_CONNECTING:
		break;
	case MCONN_QUIESCENT:
	case MCONN_WRITING:
		if (STAILQ_EMPTY(&conn->pending_head)) {
			conn->state = MCONN_QUIESCENT;
		}
		else {
			conn->cnt = 0;
			conn->inbound_msg = NULL;
			conn->inbound_tr = NULL;
			conn->state = MCONN_WRITING;
		}
		break;
	default:
		/* do nothing */
		break;
	}
	glitch_log("%p: old_state=%d, new_state=%d\n",
		   conn->msgr, old_state, conn->state);

	/* activate/deactivate io callback based on state */
	switch (conn->state) {
	case MCONN_CONNECTING:
	case MCONN_WRITING:
		ev_io_start(conn->msgr->loop, &conn->w_write);
		ev_io_stop(conn->msgr->loop, &conn->w_read);
		break;
	case MCONN_READING_HDR:
	case MCONN_READING:
	case MCONN_QUIESCENT:
		ev_io_stop(conn->msgr->loop, &conn->w_write);
		ev_io_start(conn->msgr->loop, &conn->w_read);
		break;
	}
}

static void mconn_writable_cb(POSSIBLY_UNUSED(struct ev_loop *loop),
		struct ev_io *w, int revents)
{
	struct mconn *conn = GET_OUTER(w, struct mconn, w_write);
	struct msgr *msgr = conn->msgr;

	if (revents & EV_ERROR) {
		mconn_teardown(conn, ENOMEDIUM);
		return;
	}
	if (!(revents & EV_WRITE))
		return;
	switch (conn->state) {
	case MCONN_CONNECTING: {
		int val = 0, ret;
		socklen_t val_len = sizeof(val);
		ret = getsockopt(conn->sock, SOL_SOCKET, SO_ERROR,
				&val, &val_len);
		if ((ret == 0) && (val != 0)) {
			ret = val;
		}
		if (ret) {
			mconn_teardown(conn, ret);
			return;
		}
		conn->state = MCONN_QUIESCENT;
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
			glitch_log("mconn_writable_cb: internal error: in "
				"state MCONN_WRITING, but there is no "
				"transactor pending?\n");
			conn->state = MCONN_QUIESCENT;
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
			mconn_teardown(conn, ret);
			return;
		}
		conn->cnt += res;
		if (conn->cnt == full) {
			conn->cnt = -1;
			STAILQ_REMOVE_HEAD(&conn->pending_head, u.pending_entry);
			conn->state = MCONN_QUIESCENT;
			free(tr->m);
			tr->m = NULL;
			msgr->cb(conn, tr, NULL);
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

	if (revents & EV_ERROR) {
		mconn_teardown(conn, ENOMEDIUM);
		return;
	}
	if (!(revents & EV_READ))
		return;
	switch (conn->state) {
	case MCONN_QUIESCENT:
		conn->state = MCONN_READING_HDR;
		conn->inbound_msg = calloc(1, sizeof(struct msg));
		if (!conn->inbound_msg) {
			glitch_log("%p: read_cb: OOM\n", msgr);
			mconn_inbound_abort(conn);
			return;
		}
		conn->cnt = 0;
		/* fall through */
	case MCONN_READING_HDR: {
		amt = sizeof(struct msg) - conn->cnt;
		res = read(conn->sock, conn->inbound_msg, amt);
		if (res < 0) {
			int ret = errno;
			if (is_temporary_socket_error(ret))
				return;
			mconn_teardown(conn, ret);
			return;
		}
		conn->cnt += res;
		if (conn->cnt != sizeof(struct msg))
			return;
		m_len = be32toh(conn->inbound_msg->len);
		m = realloc(conn->inbound_msg, sizeof(struct msg) + m_len);
		if (!m) {
			glitch_log("%p: read_cb: OOM on msg alloc (len=%d)\n",
				msgr, m_len);
			mconn_inbound_abort(conn);
			return;
		}
		conn->inbound_msg = m;
		conn->cnt = 0;
		tr = mtran_lookup_by_id(conn, be32toh(conn->inbound_msg->tran_id));
		if (!tr) {
			tr = mtran_alloc(msgr);
			if (!tr) {
				glitch_log("%p: read_cb: OOM\n", msgr);
				mconn_inbound_abort(conn);
				return;
			}
			tr->ip = conn->remote_ip;
			tr->port = conn->remote_port;
			RB_INSERT(active_tr, &conn->active_head, tr);
		}
		conn->inbound_tr = tr;
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
			mconn_teardown(conn, ret);
			return;
		}
		conn->cnt += res;
		if (conn->cnt != m_len) {
			return;
		}
		RB_REMOVE(active_tr, &conn->active_head, conn->inbound_tr);
		conn->state = MCONN_QUIESCENT;
		m = conn->inbound_msg;
		conn->inbound_msg = NULL;
		conn->cnt = -1;
		tr = conn->inbound_tr;
		conn->inbound_tr = NULL;
		msgr->cb(conn, tr, m);
		mconn_next_state_logic(conn);
		return;
	default:
		/* Probably should never get here */
		mconn_next_state_logic(conn);
		return;
	}
}

/****************************** mtran ********************************/
struct msgr *msgr_init(char *err, size_t err_len,
		int max_conn, int max_tran, size_t tran_sz, msgr_cb_t cb)
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
	msgr->next_tran_id = random();
	msgr->cur_tran = 0;
	msgr->max_tran = max_tran;
	msgr->cur_conn = 0;
	msgr->max_conn = max_conn;
	RB_INIT(&msgr->conn_head);
	ev_init(&msgr->w_listen_fd, NULL);
	STAILQ_INIT(&msgr->pending_head);
	ev_async_init(&msgr->w_notify, run_msgr_notify_cb);
	msgr->loop = ev_loop_new(0);
	if (!msgr->loop) {
		snprintf(err, err_len, "init_msgr: ev_loop_new failed.");
		msgr_shutdown(msgr);
		return NULL;
	}
	ev_async_start(msgr->loop, &msgr->w_notify);
	return msgr;
}

void msgr_shutdown(struct msgr *msgr)
{
	int res, need_join = 0;

	pthread_spin_lock(&msgr->lock);
	if (msgr->state == MSGR_STATE_THREAD_STARTED) {
		msgr->state = MSGR_STATE_THREAD_STOPPING;
		need_join = 1;
	}
	pthread_spin_unlock(&msgr->lock);
	if (need_join) {
		ev_async_send(msgr->loop, &msgr->w_notify);
		pthread_join(msgr->thread, NULL);
	}
	pthread_spin_destroy(&msgr->lock);
	ev_io_stop(msgr->loop, &msgr->w_listen_fd);
	ev_async_stop(msgr->loop, &msgr->w_notify);
	ev_loop_destroy(msgr->loop);
	if (msgr->listen_fd > 0)
		RETRY_ON_EINTR(res, close(msgr->listen_fd));
	free(msgr);
}

void msgr_listen(struct msgr *msgr, int port, char *err, size_t err_len)
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
	char addr_str[INET_ADDRSTRLEN];

	glitch_log("run_msgr_listen_fd_cb\n");

	msgr = GET_OUTER(w, struct msgr, w_listen_fd);
	if (revents & EV_ERROR) {
		glitch_log("%p: listen_cb: EV_ERROR\n", msgr);
		return;
	}
	if (!(revents & EV_READ)) {
		glitch_log("%p: listen_cb: no EV_READ\n", msgr);
		return;
	}
	fd = do_accept(msgr->listen_fd, (struct sockaddr*)&remote,
		sizeof(remote), WANT_O_CLOEXEC | WANT_O_NONBLOCK);
	if (fd < 0) {
		if (is_temporary_socket_error(-fd))
			return;
		glitch_log("%p: listen_cb: accept error %d\n", msgr, -fd);
		goto error;
	}
	ip = ntohl(remote.sin_addr.s_addr);
	port = ntohs(remote.sin_port);
	ipv4_to_str(ip, addr_str, sizeof(addr_str));
	conn = mconn_find(msgr, ip, port);
	if (conn) {
		glitch_log("%p: listen_cb: we already have connection %p "
			"from %s:%d\n", msgr, conn, addr_str, port);
		goto error;
	}
	conn = mconn_create(msgr, ip, port, fd);
	if (IS_ERR(conn)) {
		glitch_log("%p: listen_cb: error creating connection for "
			"(%s:%d): error %d\n", msgr, addr_str, port,
			PTR_ERR(conn));
		goto error;
	}
	glitch_log("%p: listen_cb: created conn %p for incoming "
		   "(%s:%d)\n", msgr, conn, addr_str, port);
	mconn_next_state_logic(conn);
	return;

error:
	if (fd > 0) {
		RETRY_ON_EINTR(ret, close(fd));
	}
}

static void run_msgr_setup_pending(struct msgr *msgr, struct mtran *tr)
{
	char addr_str[INET_ADDRSTRLEN];
	struct mconn *conn;

	ipv4_to_str(tr->ip, addr_str, sizeof(addr_str));
	conn = mconn_find(msgr, tr->ip, tr->port);
	if (!conn) {
		conn = mconn_create(msgr, tr->ip, tr->port, -1);
		if (IS_ERR(conn))
			return;
		glitch_log("%p: created conn %p for pending "
			   "(%s:%d)\n", msgr, conn, addr_str, tr->port);
	}
	else {
		glitch_log("%p: using conn %p for "
			   "(%s:%d)\n", msgr, conn, addr_str, tr->port);
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
		tr = STAILQ_FIRST(&msgr->pending_head);
		if (tr) {
			STAILQ_REMOVE_HEAD(&msgr->pending_head,
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

static void* run_msgr(void *v)
{
	struct msgr *msgr = (struct msgr*)v;

	ev_loop(msgr->loop, 0);
	return NULL;
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
		glitch_log("listening on fd %d\n", msgr->listen_fd);
		ev_io_init(&msgr->w_listen_fd, run_msgr_listen_fd_cb,
			msgr->listen_fd, EV_WRITE | EV_READ);
		ev_io_start(msgr->loop, &msgr->w_listen_fd);
	}
	ret = pthread_create(&msgr->thread, NULL, run_msgr, msgr);
	if (ret) {
		snprintf(err, err_len, "msgr_start: pthread_create failed "
			 "with error %d", ret);
		return;
	}
	msgr->state = MSGR_STATE_THREAD_STARTED;
}
