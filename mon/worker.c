/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "mon/worker.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WORKERS 8192

#define WORKER_SUCCESS ((void*)(intptr_t)0)
#define WORKER_ERROR ((void*)(intptr_t)1)

/** Represents the state of a worker thread
 */
enum worker_state_t
{
	WORKER_STATE_UNINITIALIZED = 0,
	WORKER_STATE_RUNNING = 1,
	WORKER_STATE_STOPPED = 2,
	WORKER_STATE_STOPPED_ERROR = 3,
};

/** Represents a worker thread.
 */
struct worker
{
	/** The next free worker in the worker free list */
	struct worker *next_free_worker;

	/** The name of this worker. It doesn't have to be unique; it's mainly
	 * for debugging */
	char name[WORKER_NAME_MAX];

	/** The lock that protects this worker's queue and state */
	pthread_mutex_t lock;

	/** The worker thread */
	pthread_t thread;

	/** The worker state */
	enum worker_state_t state;

	/** Pointer to the head of the message queue */
	struct worker_msg *msg_head;

	/** Pointer to the tail of the message queue */
	struct worker_msg *msg_tail;

	/** Condition variable used to wait for a message */
	pthread_cond_t cond;
};

struct worker_thread_param
{
	struct worker *worker;
	worker_fn_t fn;
	worker_shutdown_fn_t sfn;
	void *data;
};

/** Actual storage for workers */
static struct worker g_workers[MAX_WORKERS];

/** The head of the free workers list, or NULL. */
static struct worker* g_next_free_worker;

/** Protects the free workers list */
static pthread_mutex_t g_next_free_worker_lock;

void* worker_main(void *v)
{
	int done = 0;
	struct worker_msg *msg;
	void *ret = WORKER_SUCCESS;
	struct worker_thread_param *wtp = (struct worker_thread_param *)v;
	struct worker *worker = wtp->worker;
	worker_fn_t fn = wtp->fn;
	worker_shutdown_fn_t sfn = wtp->sfn;
	void *data = wtp->data;
	free(wtp);

	do {
		int res;
		pthread_mutex_lock(&worker->lock);
		while (!worker->msg_head) {
			pthread_cond_wait(&worker->cond, &worker->lock);
		}
		msg = worker->msg_head;
		if (msg->next) {
			worker->msg_head = msg->next;
		}
		else {
			worker->msg_head = NULL;
			worker->msg_tail = NULL;
		}
		msg->next = NULL;
		pthread_mutex_unlock(&worker->lock);
		switch (msg->ty) {
		case WORKER_MSG_SHUTDOWN:
			ret = WORKER_SUCCESS;
			done = 1;
			break;
		default:
			res = fn(msg, data);
			if (res != 0) {
				ret = WORKER_ERROR;
				done = 1;
				break;
			}
		}
		free(msg);
	} while (!done);
	pthread_mutex_lock(&worker->lock);
	if (ret == WORKER_SUCCESS)
		worker->state = WORKER_STATE_STOPPED;
	else
		worker->state = WORKER_STATE_STOPPED_ERROR;
	for (msg = worker->msg_head; msg; ) {
		struct worker_msg *next = msg->next;
		free(msg);
		msg = next;
	}
	worker->msg_head = worker->msg_tail = NULL;
	pthread_mutex_unlock(&worker->lock);
	/* Invoke shutdown function, if it's present. */
	if (sfn) {
		sfn(data);
	}
	return ret;
}

int worker_init(void)
{
	int ret, i;
	memset(g_workers, 0, sizeof(struct worker) * MAX_WORKERS);
	pthread_mutex_init(&g_next_free_worker_lock, NULL);
	for (i = 0; i < MAX_WORKERS; ++i) {
		ret = pthread_mutex_init(&g_workers[i].lock, NULL);
		if (ret) {
			for (--i; i > 0; --i) {
				pthread_mutex_destroy(&g_workers[i].lock);
			}
			pthread_mutex_destroy(&g_next_free_worker_lock);
			return ret;
		}
		ret = pthread_cond_init(&g_workers[i].cond, NULL);
		if (ret) {
			pthread_mutex_destroy(&g_workers[i].lock);
			for (--i; i > 0; --i) {
				pthread_mutex_destroy(&g_workers[i].lock);
				pthread_cond_destroy(&g_workers[i].cond);
			}
			pthread_mutex_destroy(&g_next_free_worker_lock);
			return ret;
		}
	}
	for (i = 0; i < MAX_WORKERS; ++i) {
		g_workers[i].next_free_worker = &g_workers[i + 1];
	}
	g_next_free_worker = &g_workers[0];
	g_workers[MAX_WORKERS - 1].next_free_worker = NULL;
	return 0;
}

struct worker *worker_start(const char *name, worker_fn_t fn,
			    worker_shutdown_fn_t sfn, void *data)
{
	int ret;
	struct worker_thread_param *wtp;
	struct worker *worker;
	enum worker_state_t old_state;

	wtp = calloc(1, sizeof(struct worker_thread_param));
	if (!wtp) {
		return NULL;
	}
	pthread_mutex_lock(&g_next_free_worker_lock);
	if (g_next_free_worker == NULL) {
		pthread_mutex_unlock(&g_next_free_worker_lock);
		free(wtp);
		return NULL;
	}
	worker = g_next_free_worker;
	g_next_free_worker = worker->next_free_worker;
	worker->next_free_worker = NULL;
	snprintf(worker->name, WORKER_NAME_MAX, "%s", name);
	old_state = worker->state;
	worker->state = WORKER_STATE_RUNNING;
	worker->msg_head = NULL;
	worker->msg_tail = NULL;
	wtp->worker = worker;
	wtp->fn = fn;
	wtp->sfn = sfn;
	wtp->data = data;
	ret = pthread_create(&worker->thread, NULL, worker_main, wtp);
	if (ret) {
		worker->name[0] = '\0';
		worker->state = old_state;
		worker->next_free_worker = g_next_free_worker;
		g_next_free_worker = worker;
		pthread_mutex_unlock(&g_next_free_worker_lock);
		return NULL;
	}
	pthread_mutex_unlock(&g_next_free_worker_lock);
	return worker;
}

int worker_sendmsg(struct worker *worker, void *m)
{
	struct worker_msg *msg = (struct worker_msg *)m;
	pthread_mutex_lock(&worker->lock);
	if (worker->state != WORKER_STATE_RUNNING) {
		pthread_mutex_unlock(&worker->lock);
		return -EINVAL;
	}
	if (worker->msg_tail) {
		worker->msg_tail->next = msg;
		worker->msg_tail = msg;
	}
	else {
		worker->msg_head = msg;
		worker->msg_tail = msg;
	}
	pthread_cond_signal(&worker->cond);
	pthread_mutex_unlock(&worker->lock);
	return 0;
}

int worker_sendmsg_or_free(struct worker *worker, void *m)
{
	int ret = worker_sendmsg(worker, m);
	if (ret) {
		free(m);
	}
	return ret;
}

int worker_stop(struct worker *worker)
{
	struct worker_msg *msg = calloc(1, sizeof(struct worker_msg));
	if (!msg) {
		return -ENOMEM;
	}
	msg->ty = WORKER_MSG_SHUTDOWN;
	return worker_sendmsg(worker, msg);
}

int worker_join(struct worker *worker)
{
	int ret;
	void *retval;
	pthread_t thread;
	pthread_mutex_lock(&worker->lock);
	if (worker->state == WORKER_STATE_UNINITIALIZED) {
		pthread_mutex_unlock(&worker->lock);
		return -EINVAL;
	}
	thread = worker->thread;
	pthread_mutex_unlock(&worker->lock);
	ret = pthread_join(thread, &retval);
	if (ret) {
		return ret;
	}
	worker->name[0] = '\0';

	pthread_mutex_lock(&g_next_free_worker_lock);
	worker->next_free_worker = g_next_free_worker;
	g_next_free_worker = worker;
	pthread_mutex_unlock(&g_next_free_worker_lock);

	if (retval == WORKER_SUCCESS) {
		return 0;
	}
	else {
		return 1;
	}
}
