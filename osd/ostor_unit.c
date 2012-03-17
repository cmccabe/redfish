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

#include "common/config/ostorc.h"
#include "core/process_ctx.h"
#include "osd/ostor.h"
#include "util/error.h"
#include "util/string.h"
#include "util/tempfile.h"
#include "util/test.h"

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

static int ostoru_test1(const char *ostor_path)
{
	struct ostorc *oconf;
	struct ostor *ostor;
	int32_t amt;
	char buf[1024];

	oconf = JORM_INIT_ostorc();
	EXPECT_NOT_ERRPTR(oconf);
	oconf->ostor_max_open = 10;
	oconf->ostor_timeo = 10;
	oconf->ostor_path = strdup(ostor_path);
	EXPECT_NOT_EQ(oconf->ostor_path, NULL);
	ostor = ostor_init(oconf);
	EXPECT_NOT_ERRPTR(ostor);
	amt = ostor_write(ostor, 123, TEST_DATA1, strlen(TEST_DATA1));
	printf("amt = %d\n", amt);
	EXPECT_ZERO(amt);
	amt = ostor_write(ostor, 456, TEST_DATA2, strlen(TEST_DATA2));
	EXPECT_ZERO(amt);
	EXPECT_ZERO(ostor_write(ostor, 789, TEST_DATA3, strlen(TEST_DATA3)));
	memset(buf, 0, sizeof(buf));
	amt = ostor_read(ostor, 123, 0, buf, sizeof(buf));
	EXPECT_EQ(amt, strlen(TEST_DATA1));
	EXPECT_ZERO(memcmp(buf, TEST_DATA1, strlen(TEST_DATA1)));
	amt = ostor_read(ostor, 456, 1, buf, sizeof(buf));
	EXPECT_EQ(amt, strlen(TEST_DATA2) - 1);
	EXPECT_ZERO(memcmp(buf, TEST_DATA2 + 1, strlen(TEST_DATA2) - 1));
	amt = ostor_read(ostor, 333, 0, buf, sizeof(buf));
	EXPECT_EQ(amt, -ENOENT);
	amt = ostor_unlink(ostor, 123);
	EXPECT_ZERO(amt);
	amt = ostor_read(ostor, 123, 0, buf, sizeof(buf));
	EXPECT_EQ(amt, -ENOENT);
	ostor_shutdown(ostor);
	ostor_free(ostor);
	JORM_FREE_ostorc(oconf);
	return 0;
}

int main(POSSIBLY_UNUSED(int argc), char **argv)
{
	char tdir[PATH_MAX], tdir2[PATH_MAX];

	EXPECT_ZERO(utility_ctx_init(argv[0])); /* for g_fast_log_mgr */

	EXPECT_ZERO(get_tempdir(tdir, sizeof(tdir), 0755));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir));
	EXPECT_ZERO(ostoru_test_open_close(tdir));

	EXPECT_ZERO(get_tempdir(tdir2, sizeof(tdir2), 0755));
	EXPECT_ZERO(register_tempdir_for_cleanup(tdir2));
	EXPECT_ZERO(ostoru_test1(tdir2));

	process_ctx_shutdown();

	return EXIT_SUCCESS;
}
