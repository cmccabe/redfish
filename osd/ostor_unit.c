/*
 * vim: ts=8:sw=8:tw=79:noet
 * 
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

#include "core/alarm.h"
#include "common/config/ostorc.h"
#include "core/process_ctx.h"
#include "osd/ostor.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"
#include "util/thread.h"
#include "util/time.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char TEST_DATA1[] = "1234567890";

static const char TEST_DATA2[] = "here is some test data!";

static const char TEST_DATA3[] = "";

static int ostoru_test_open_close(const char *ostor_path)
{
	struct ostorc *oconf;
	struct ostor *ostor;

	oconf = JORM_INIT_ostorc();
	EXPECT_NOT_ERRPTR(oconf);
	oconf->ostor_max_open = 10;
	oconf->ostor_timeo = 10;
	oconf->ostor_path = strdup(ostor_path);
	EXPECT_NOT_EQ(oconf->ostor_path, NULL);
	ostor = ostor_init(oconf);
	EXPECT_NOT_ERRPTR(ostor);
	ostor_shutdown(ostor);
	ostor_free(ostor);
	JORM_FREE_ostorc(oconf);
	return 0;
}

static int ostoru_simple_test(const char *ostor_path, struct fast_log_buf *fb,
		int max_open)
{
	struct ostorc *oconf;
	struct ostor *ostor;
	int32_t amt;
	char buf[1024];

	oconf = JORM_INIT_ostorc();
	EXPECT_NOT_ERRPTR(oconf);
	oconf->ostor_max_open = max_open;
	oconf->ostor_timeo = 10;
	oconf->ostor_path = strdup(ostor_path);
	EXPECT_NOT_EQ(oconf->ostor_path, NULL);
	ostor = ostor_init(oconf);
	EXPECT_NOT_ERRPTR(ostor);
	EXPECT_ZERO(ostor_write(ostor, fb, 123, TEST_DATA1,
			strlen(TEST_DATA1)));
	EXPECT_ZERO(ostor_write(ostor, fb, 456, TEST_DATA2,
			strlen(TEST_DATA2)));
	EXPECT_ZERO(ostor_write(ostor, fb, 789, TEST_DATA3,
			strlen(TEST_DATA3)));
	memset(buf, 0, sizeof(buf));
	amt = ostor_read(ostor, fb, 123, 0, buf, sizeof(buf));
	EXPECT_EQ(amt, strlen(TEST_DATA1));
	EXPECT_ZERO(memcmp(buf, TEST_DATA1, strlen(TEST_DATA1)));
	amt = ostor_read(ostor, fb, 456, 1, buf, sizeof(buf));
	EXPECT_EQ(amt, strlen(TEST_DATA2) - 1);
	EXPECT_ZERO(memcmp(buf, TEST_DATA2 + 1, strlen(TEST_DATA2) - 1));
	amt = ostor_read(ostor, fb, 333, 0, buf, sizeof(buf));
	EXPECT_EQ(amt, -ENOENT);
	EXPECT_ZERO(ostor_unlink(ostor, fb, 123));
	EXPECT_EQ(ostor_unlink(ostor, fb, 123), -ENOENT);
	amt = ostor_read(ostor, fb, 123, 0, buf, sizeof(buf));
	EXPECT_EQ(amt, -ENOENT);
	amt = ostor_read(ostor, fb, 789, 0, buf, sizeof(buf));
	EXPECT_EQ(amt, 0);
	ostor_shutdown(ostor);
	ostor_free(ostor);
	JORM_FREE_ostorc(oconf);
	return 0;
}

static sem_t ostoru_threaded_test_sem1;
static sem_t ostoru_threaded_test_sem2;

static int ostoru_thread1(struct redfish_thread *rt)
{
	char buf[1024];
	struct ostor *ostor = rt->priv;

	EXPECT_ZERO(ostor_write(ostor, rt->fb, 123, TEST_DATA1,
		strlen(TEST_DATA1)));
	sem_post(&ostoru_threaded_test_sem2);
	sem_wait(&ostoru_threaded_test_sem1);
	EXPECT_EQ(ostor_read(ostor, rt->fb, 123, 0, buf, sizeof(buf)),
		-ENOENT);
	EXPECT_EQ(ostor_unlink(ostor, rt->fb, 123), -ENOENT);
	EXPECT_ZERO(ostor_write(ostor, rt->fb, 456, TEST_DATA2,
		strlen(TEST_DATA2)));
	sem_post(&ostoru_threaded_test_sem2);
	sem_wait(&ostoru_threaded_test_sem1);
	ostor_shutdown(ostor);
	sem_post(&ostoru_threaded_test_sem2);
	return 0;
}

static int ostoru_thread2(struct redfish_thread *rt)
{
	char buf[1024];
	struct ostor *ostor = rt->priv;

	sem_wait(&ostoru_threaded_test_sem2);
	EXPECT_EQ(ostor_read(ostor, rt->fb, 123, 0, buf, sizeof(buf)),
		strlen(TEST_DATA1));
	EXPECT_ZERO(memcmp(buf, TEST_DATA1, strlen(TEST_DATA1)));
	EXPECT_EQ(ostor_read(ostor, rt->fb, 123, 0, buf, 1), 1);
	EXPECT_ZERO(memcmp(buf, TEST_DATA1, 1));
	EXPECT_ZERO(ostor_unlink(ostor, rt->fb, 123));
	sem_post(&ostoru_threaded_test_sem1);
	sem_wait(&ostoru_threaded_test_sem2);
	EXPECT_ZERO(ostor_write(ostor, rt->fb, 456, TEST_DATA2,
		strlen(TEST_DATA2)));
	EXPECT_EQ(ostor_read(ostor, rt->fb, 456, 0, buf, sizeof(buf)),
		2 * strlen(TEST_DATA2));
	EXPECT_ZERO(memcmp(buf, TEST_DATA2, strlen(TEST_DATA2)));
	EXPECT_ZERO(memcmp(buf + strlen(TEST_DATA2), TEST_DATA2,
		strlen(TEST_DATA2)));
	sem_post(&ostoru_threaded_test_sem1);
	sem_wait(&ostoru_threaded_test_sem2);
	EXPECT_EQ(ostor_read(ostor, rt->fb, 456, 0, buf, sizeof(buf)),
		-ESHUTDOWN);
	return 0;
}

static int ostoru_threaded_test(const char *ostor_path, int max_open)
{
	struct ostorc *oconf;
	struct ostor *ostor;
	struct redfish_thread thread1, thread2;

	sem_init(&ostoru_threaded_test_sem1, 0, 0);
	sem_init(&ostoru_threaded_test_sem2, 0, 0);
	oconf = JORM_INIT_ostorc();
	EXPECT_NOT_ERRPTR(oconf);
	oconf->ostor_max_open = max_open;
	oconf->ostor_timeo = 10;
	oconf->ostor_path = strdup(ostor_path);
	EXPECT_NOT_EQ(oconf->ostor_path, NULL);
	ostor = ostor_init(oconf);
	EXPECT_NOT_ERRPTR(ostor);
	EXPECT_ZERO(redfish_thread_create(g_fast_log_mgr, &thread1,
			ostoru_thread1, ostor));
	EXPECT_ZERO(redfish_thread_create(g_fast_log_mgr, &thread2,
			ostoru_thread2, ostor));
	EXPECT_ZERO(redfish_thread_join(&thread1));
	EXPECT_ZERO(redfish_thread_join(&thread2));
	EXPECT_ZERO(sem_destroy(&ostoru_threaded_test_sem1));
	EXPECT_ZERO(sem_destroy(&ostoru_threaded_test_sem2));
	ostor_free(ostor);
	JORM_FREE_ostorc(oconf);
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	char tdir[PATH_MAX];
	struct fast_log_buf *fb;
	timer_t timer;
	time_t t;

	EXPECT_ZERO(utility_ctx_init(argv[0])); /* for g_fast_log_mgr */
	t = mt_time() + 600;
	EXPECT_ZERO(mt_set_alarm(t, "ostor_unit timed out", &timer));
	fb = fast_log_create(g_fast_log_mgr, "main");
	EXPECT_NOT_ERRPTR(fb);

	EXPECT_ZERO(get_tempdir(tdir, sizeof(tdir), 0755));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));
	EXPECT_ZERO(ostoru_test_open_close(tdir));

	EXPECT_ZERO(get_tempdir(tdir, sizeof(tdir), 0755));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));
	EXPECT_ZERO(ostoru_simple_test(tdir, fb, 100));

	EXPECT_ZERO(get_tempdir(tdir, sizeof(tdir), 0755));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));
	EXPECT_ZERO(ostoru_simple_test(tdir, fb, 1));

	EXPECT_ZERO(get_tempdir(tdir, sizeof(tdir), 0755));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));
	EXPECT_ZERO(ostoru_threaded_test(tdir, 10));

	EXPECT_ZERO(mt_deactivate_alarm(timer));
	fast_log_free(fb);
	process_ctx_shutdown();

	return EXIT_SUCCESS;
}
