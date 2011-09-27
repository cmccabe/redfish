/*
 * The OneFish distributed filesystem
 *
 * Copyright (C) 2011 Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "core/glitch_log.h"
#include "rsem/rsem.h"
#include "rsem/rsem_cli.h"
#include "rsem/rsem_srv.h"
#include "util/error.h"
#include "util/msleep.h"
#include "util/test.h"

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RSEM_UNIT_SRV_PORT 30000
#define RSEM_UNIT_CLI_PORT_START 30001
#define RSEM_UNIT_CLI_PORT_END 30005

static int test1(struct rsem_client *rcli)
{
	alarm(300);
	rsem_post(rcli, "foo");
	EXPECT_ZERO(rsem_wait(rcli, "foo"));
	alarm(0);
	return 0;
}

static int test2(struct rsem_client *rcli)
{
	alarm(300);
	EXPECT_ZERO(rsem_wait(rcli, "bar"));
	EXPECT_ZERO(rsem_wait(rcli, "bar"));
	rsem_post(rcli, "bar");
	EXPECT_ZERO(rsem_wait(rcli, "bar"));
	rsem_post(rcli, "bar");
	EXPECT_ZERO(rsem_wait(rcli, "bar"));
	rsem_post(rcli, "bar");
	rsem_post(rcli, "bar");
	alarm(0);
	return 0;
}

static volatile int g_test3a_mcguffin = 0;

static sem_t g_test3a_local_sem;

static void* test3a(void *v)
{
	struct rsem_client *rcli = (struct rsem_client*)v;
	sem_wait(&g_test3a_local_sem);
	g_test3a_mcguffin = 1;
	do_msleep(10);
	rsem_post(rcli, "baz");
	return NULL;
}

static int test3(struct rsem_client *rcli)
{
	pthread_t thread;
	EXPECT_ZERO(sem_init(&g_test3a_local_sem, 0, 0));
	EXPECT_ZERO(pthread_create(&thread, NULL, test3a, rcli));
	sem_post(&g_test3a_local_sem);
	EXPECT_ZERO(rsem_wait(rcli, "baz"));
	EXPECT_EQUAL(g_test3a_mcguffin, 1);
	EXPECT_ZERO(pthread_join(thread, NULL));
	EXPECT_ZERO(sem_destroy(&g_test3a_local_sem));
	return 0;
}

int main(void)
{
	char err[512] = { 0 };
	struct rsem_client_conf cli_conf;
	struct rsem_client* rcli;
	struct rsem_server_conf srv_conf;
	struct rsem_server* rss;
	struct rsem_conf rsem_conf[] = {
		{
			.name = "foo",
			.init_val = 1,
		},
		{
			.name = "bar",
			.init_val = 2,
		},
		{
			.name = "baz",
			.init_val = 0,
		},
	};
	struct rsem_conf *rsem_conf_arr[] =
		{ &rsem_conf[0], &rsem_conf[1], &rsem_conf[2], NULL };
	
	signal(SIGPIPE, SIG_IGN);
	memset(&cli_conf, 0, sizeof(cli_conf));
	cli_conf.srv_port = RSEM_UNIT_SRV_PORT;
	cli_conf.srv_host = "localhost";
	cli_conf.cli_port_start = RSEM_UNIT_CLI_PORT_START;
	cli_conf.cli_port_end = RSEM_UNIT_CLI_PORT_END;
	rcli = rsem_client_init(&cli_conf, err, sizeof(err));
	if (err[0]) {
		glitch_log("rsem_client_init error: %s\n", err);
		return EXIT_FAILURE;
	}
	memset(&srv_conf, 0, sizeof(srv_conf));
	srv_conf.port = RSEM_UNIT_SRV_PORT;
	srv_conf.sems = rsem_conf_arr;
	rss = start_rsem_server(&srv_conf, err, sizeof(err));
	if (err[0]) {
		glitch_log("rsem_server error: %s\n", err);
		return EXIT_FAILURE;
	}
	EXPECT_ZERO(test1(rcli));
	EXPECT_ZERO(test2(rcli));
	EXPECT_ZERO(test3(rcli));
	rsem_server_shutdown(rss);
	rsem_client_destroy(rcli);
	return EXIT_SUCCESS;
}
