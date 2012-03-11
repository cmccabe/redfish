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

#include "common//config/mstorc.h"
#include "mds/limits.h"

#define JORM_CUR_FILE "common/config/mstorc.jorm"
#include "jorm/jorm_generate_body.h"
#undef JORM_CUR_FILE

#define DEFAULT_MSTOR_CACHE_MB 1024
#define DEFAULT_MSTOR_IO_THREADS 16
#define DEFAULT_MIN_ZOMBIE_TIME 60
#define DEFAULT_MIN_REPL 3
#define DEFAULT_MAN_REPL 3

/** If sizeof(size_t) == 4, we could overflow when computing the user's desired
 * cache size.  Basically, this would only happen on a 32-bit machine.
 * In this case, limit the cache to 4 GB, since that's all you get on a 32-bit
 * machine anyway.
 *
 * Also note that size_t is always unsigned, so overflow is not a concern here.
 *
 * @param cache_mb	(inout) the cache size in megabytes
 */
static void correct_mstor_cache_mb_overflow(int *cache_mb)
{
	size_t a;
	uint64_t b;

	a = *cache_mb;
	a *= 1024 * 1024;
	b = *cache_mb;
	b *= 1024 * 1024;
	if (a != b) {
		*cache_mb = 4096;
	}
}

void harmonize_mstorc(struct mstorc *conf, char *err, size_t err_len)
{
	if (conf->mstor_path == JORM_INVAL_STR) {
		snprintf(err, err_len, "you must give a path to the mstor");
		return;
	}
	if (conf->mstor_cache_mb == JORM_INVAL_INT)
		conf->mstor_cache_mb = DEFAULT_MSTOR_CACHE_MB;
	correct_mstor_cache_mb_overflow(&conf->mstor_cache_mb);
	if (conf->mstor_io_threads == JORM_INVAL_INT)
		conf->mstor_io_threads = DEFAULT_MSTOR_IO_THREADS;
	if (conf->min_zombie_time == JORM_INVAL_INT)
		conf->min_zombie_time = DEFAULT_MIN_ZOMBIE_TIME;
	if (conf->mstor_create == JORM_INVAL_BOOL)
		conf->mstor_create = 1;
	if (conf->min_repl == JORM_INVAL_INT)
		conf->min_repl = DEFAULT_MIN_REPL;
	else if ((conf->min_repl < 0) || (conf->min_repl > RF_MAX_OID)) {
		snprintf(err, err_len, "you cannot configure a "
			"min_repl of %d", conf->min_repl);
		return;
	}
	if (conf->man_repl == JORM_INVAL_INT)
		conf->man_repl = DEFAULT_MAN_REPL;
	else if ((conf->man_repl < 0) || (conf->man_repl > RF_MAX_OID)) {
		snprintf(err, err_len, "you cannot configure a "
			"man_repl of %d", conf->man_repl);
		return;
	}
	else if (conf->man_repl < conf->min_repl) {
		snprintf(err, err_len, "error: default mandatory "
			 "replication is less than default minimum "
			 "replication");
		return;
	}
}
