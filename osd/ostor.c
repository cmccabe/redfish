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

#include "common/cluster_map.h"
#include "common/config/ostorc.h"
#include "common/config/unitaryc.h"
#include "core/glitch_log.h"
#include "core/process_ctx.h"
#include "jorm/jorm_const.h"
#include "mds/limits.h"
#include "osd/fast_log.h"
#include "osd/ostor.h"
#include "util/compiler.h"
#include "util/error.h"
#include "util/fast_log.h"
#include "util/fast_log_types.h"
#include "util/safe_io.h"
#include "util/string.h"
#include "util/terror.h"
#include "util/thread.h"
#include "util/time.h"
#include "util/tree.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define OSTOR_LRU_LONG_PERIOD_SEC 60

#define OSTOR_LRU_SHORT_PERIOD_NSEC 1000

#define OSTOR_TEST_DIR "test.tmp"

struct ochunk {
	RB_ENTRY(ochunk) by_cid_entry;
	RB_ENTRY(ochunk) by_atime_entry;
	/** chunk id */
	uint64_t cid;
	/** last access time */
	time_t atime;
	/** open file descriptor */
	int fd;
	/** Reference count.  If this is -1, the chunk is in the process of
	 * being created or being destroyed.  */
	int32_t refcnt;
};

static int compare_ochunk_by_cid(struct ochunk *ch_a,
		struct ochunk *ch_b) PURE;
static int compare_ochunk_by_atime(struct ochunk *ch_a,
		struct ochunk *ch_b) PURE;
static struct ochunk *ostor_get_ochunk(struct ostor *ostor,
		struct fast_log_buf *fb, uint64_t cid, int create);
static int ostor_lru_thread(struct redfish_thread *rt);

RB_HEAD(ochunks_by_cid, ochunk);
RB_GENERATE(ochunks_by_cid, ochunk, by_cid_entry, compare_ochunk_by_cid);
RB_HEAD(ochunks_by_atime, ochunk);
RB_GENERATE(ochunks_by_atime, ochunk, by_atime_entry, compare_ochunk_by_atime);

/** The backend store for the osd's data.  Basically, this is where we put chunk
 * data.
 *
 * Most of the complexity here comes from three things:
 * 1. We don't want to do an open/modify/close cycle on each operation, since
 * there is a high overhead for open and close.  So we keep an LRU
 * (least-recently-used) cache of open file descriptors.
 *
 * 2. We don't want to perform blocking system calls, like
 * read/write/open/close/unlink, while holding ostor->lock.
 *
 * 3. We also want to limit the number of open file descriptors.  Using too many
 * file descriptors could cause problems for other processes on this machine.
 * It could also lead to us geting EMFILE, which would be very awkward to handle.
 *
 * This design accomplishes all three of those things.  See ostorc.jorm for
 * tunables.
 *
 * The busy-waiting could be eliminated by using per-chunk condition variables
 * and locks, but I doubt that that would be worth it.  We only really have to
 * busy-wait when block creations and deletions are racing-- NOT a common
 * scenario.  If we had per-chunk condition variables and mutexes, we'd have to
 * pay the cost of using them all the time.
 */
struct ostor {
	/** Path to the ostor directory */
	char *dir_path;
	/** If nonzero, we are shutting down */
	int shutdown;
	/** current number of files that are open */
	int num_open;
	/** maximum number of files to open */
	int max_open;
	/** maximum number of seconds to leave a file open once it's unused */
	time_t atime_timeo;
	/** lock which protects num_open, chunk_head */
	pthread_mutex_t lock;
	/** condition variable used to signal that the garbage collector thread
	 * should wake up */
	pthread_cond_t lru_cond;
	/** minimum number of files we need to close to make way for new ones */
	int need_lru;
	/** condition variable used to signal that more ochunks are allowed to
	 * be opened */
	pthread_cond_t alloc_cond;
	/** tree of open chunks sorted by chunk id */
	struct ochunks_by_cid cid_head;
	/** tree of open chunks sorted by last access time */
	struct ochunks_by_atime atime_head;
	/** the lru thread */
	struct redfish_thread lru_thread;
};

/************************** ochunk *******************************/
static void ochunk_get_dpath(const struct ostor *ostor, char *dpath,
		size_t dpath_len, uint64_t cid)
{
	snprintf(dpath, dpath_len, "%s/%02x", ostor->dir_path,
		 (int)(cid & 0xff));
}

static void ochunk_get_path(const struct ostor *ostor, char *path,
		size_t path_len, uint64_t cid)
{
	snprintf(path, path_len, "%s/%02x/%014" PRIx64, ostor->dir_path,
		 (int)(cid & 0xff), cid >> 16);
}

/** Allocate an ostor chunk.
 *
 * Create the data structure in memory for a chunk.
 *
 * Should be called with the ostor lock held.
 *
 * @param ostor		The ostor
 * @param cid		The chunk ID
 */
static struct ochunk *ochunk_alloc(struct ostor *ostor, uint64_t cid)
{
	struct ochunk *ch;

	ch = calloc(1, sizeof(struct ochunk));
	if (!ch)
		return ERR_PTR(ENOMEM);
	ch->cid = cid;
	ch->fd = -1;
	ch->atime = 0;
	ch->refcnt = -1;
	ostor->num_open++;
	RB_INSERT(ochunks_by_cid, &ostor->cid_head, ch);
	return ch;
}

/** Open the file backing an ostor chunk.
 *
 * Should be called with the ostor lock __released__.
 *
 * @param ostor		The current monotonic time
 * @param cur_time	The current monotonic time
 * @param create	If nonzero, create the chunk if it doesn't exist.
 */
static int ochunk_open(struct ostor *ostor, struct ochunk *ch, int create)
{
	int ret, open_flags;
	char path[PATH_MAX], dpath[PATH_MAX];

	ochunk_get_path(ostor, path, sizeof(path), ch->cid);
	open_flags = create ? O_CREAT : 0;
	open_flags |= O_APPEND | O_RDWR | O_CLOEXEC | O_NOATIME;
	RETRY_ON_EINTR(ch->fd, open(path, open_flags, 0660));
	if (ch->fd >= 0)
		return 0;
	ret = errno;
	if ((ret != ENOENT) || (!create)) {
		return ret;
	}
	ochunk_get_dpath(ostor, dpath, sizeof(dpath), ch->cid);
	if (mkdir(dpath, 0770) < 0) {
		ret = -errno;
		return ret;
	}
	RETRY_ON_EINTR(ch->fd, open(path, open_flags, 0660));
	if (ch->fd >= 0)
		return 0;
	ret = -errno;
	return ret;
}

static void ochunk_release(struct ostor *ostor, struct ochunk *ch)
{
	time_t t;

	t = mt_time();
	pthread_mutex_lock(&ostor->lock);
	ch->atime = t;
	if (ch->refcnt == -1)
		abort();
	ch->refcnt--;
	RB_INSERT(ochunks_by_atime, &ostor->atime_head, ch);
	pthread_mutex_unlock(&ostor->lock);
}

/** Close the file associated with an ochunk, and flush the ochunk data
 * structure from memory.
 *
 * Should be called with the ostor lock held.
 *
 * @param ostor		The ostor
 * @param cid		The chunk ID
 */
static void ochunk_evict(struct ostor *ostor, struct fast_log_buf *fb,
		struct ochunk *ch)
{
	int res = 0, fd = ch->fd;
	uint64_t cid = ch->cid;

	if (ch->refcnt != -1)
		abort();
	if (fd > 0) {
		pthread_mutex_unlock(&ostor->lock);
		RETRY_ON_EINTR(res, close(fd));
		if (res) {
			glitch_log("ostor error: failed to close fd %d: error "
				"%d (%s)\n", fd, res, terror(res));
		}
		ch->fd = -1;
		pthread_mutex_lock(&ostor->lock);
	}
	RB_REMOVE(ochunks_by_cid, &ostor->cid_head, ch);
	free(ch);
	if (ostor->need_lru > 0)
		ostor->need_lru--;
	ostor->num_open--;
	pthread_cond_signal(&ostor->alloc_cond);
	fast_log_ostor(fb, FLOS_OCHUNK_EVICT, cid, 0, res, fd);
}

static int compare_ochunk_by_cid(struct ochunk *ch_a, struct ochunk *ch_b)
{
	if (ch_a->cid < ch_b->cid)
		return -1;
	if (ch_a->cid > ch_b->cid)
		return 1;
	return 0;
}

static int compare_ochunk_by_atime(struct ochunk *ch_a, struct ochunk *ch_b)
{
	if (ch_a->atime < ch_b->atime)
		return -1;
	if (ch_a->atime > ch_b->atime)
		return 1;
	return compare_ochunk_by_cid(ch_a, ch_b);
}

/************************** ostor *******************************/
struct ostor *ostor_init(const struct ostorc *oconf)
{
	int ret;
	struct ostor *ostor;
	char tpath[PATH_MAX];

	/* create the ostor directory if it doesn't already exist. */
	if (mkdir(oconf->ostor_path, 0770) < 0) {
		ret = errno;
		if (ret != EEXIST) {
			glitch_log("ostor_init: Failed to create directory "
				"'%s': error %d (%s)\n", oconf->ostor_path,
				ret, terror(ret));
			goto error;
		}
	}
	ret = zsnprintf(tpath, sizeof(tpath), "%s/%s",
		oconf->ostor_path, OSTOR_TEST_DIR);
	if (ret) {
		glitch_log("ostor_init: oconf->ostor_path was too long at %Zd "
			"bytes!\n", strlen(oconf->ostor_path));
		goto error;
	}
	if (mkdir(tpath, 0770) < 0) {
		ret = errno;
		glitch_log("ostor_init: failed to create directory '%s'.  "
			"Error %d: %s.  Have you set appropriate permissions "
			"on the ostor_path?\n", tpath, ret, terror(ret));
		goto error;
	}
	if (rmdir(tpath)) {
		ret = errno;
		glitch_log("ostor_init: failed to remove directory '%s'. "
			"Error %d: %s.\n", tpath, ret, terror(ret));
		goto error;
	}
	ostor = calloc(1, sizeof(struct ostor)); 
	if (!ostor) {
		ret = -ENOMEM;
		goto error;
	}
	ostor->dir_path = strdup(oconf->ostor_path);
	if (!ostor->dir_path) {
		ret = -ENOMEM;
		goto error_free_ostor;
	}
	ostor->shutdown = 0;
	ostor->num_open = 0;
	ostor->max_open = oconf->ostor_max_open;
	ostor->atime_timeo = oconf->ostor_timeo;
	ret = pthread_mutex_init(&ostor->lock, NULL);
	if (ret) {
		goto error_free_dir_path;
	}
	ret = pthread_cond_init_mt(&ostor->lru_cond);
	if (ret) {
		goto error_free_lock;
	}
	ostor->need_lru = 0;
	ret = pthread_cond_init_mt(&ostor->alloc_cond);
	if (ret) {
		goto error_free_lru_cond;
	}
	RB_INIT(&ostor->cid_head);
	RB_INIT(&ostor->atime_head);
	ret = redfish_thread_create(g_fast_log_mgr, &ostor->lru_thread,
		ostor_lru_thread, ostor);
	if (ret) {
		goto error_free_alloc_cond;
	}
	return ostor;

error_free_alloc_cond:
	pthread_cond_destroy(&ostor->alloc_cond);
error_free_lru_cond:
	pthread_cond_destroy(&ostor->lru_cond);
error_free_lock:
	pthread_mutex_destroy(&ostor->lock);
error_free_dir_path:
	free(ostor->dir_path);
error_free_ostor:
	free(ostor);
error:
	return ERR_PTR(ret);
}

void ostor_shutdown(struct ostor *ostor)
{
	pthread_mutex_lock(&ostor->lock);
	ostor->shutdown = 1;
	pthread_cond_broadcast(&ostor->lru_cond);
	pthread_cond_broadcast(&ostor->alloc_cond);
	pthread_mutex_unlock(&ostor->lock);
	redfish_thread_join(&ostor->lru_thread);
}

void ostor_free(struct ostor *ostor)
{
	pthread_cond_destroy(&ostor->alloc_cond);
	pthread_cond_destroy(&ostor->lru_cond);
	pthread_mutex_destroy(&ostor->lock);
	free(ostor->dir_path);
	free(ostor);
}

int ostor_write(struct ostor *ostor, struct fast_log_buf *fb, uint64_t cid,
		const char *data, uint32_t dlen)
{
	int ret;
	struct ochunk *ch;

	if (cid == RF_INVAL_CID) {
		ret = -EINVAL;
		goto done;
	}
	pthread_mutex_lock(&ostor->lock);
	ch = ostor_get_ochunk(ostor, fb, cid, 1);
	if (IS_ERR(ch)) {
		pthread_mutex_unlock(&ostor->lock);
		ret = FORCE_NEGATIVE(PTR_ERR(ch));
		goto done;
	}
	RB_REMOVE(ochunks_by_atime, &ostor->atime_head, ch);
	ch->refcnt++;
	pthread_mutex_unlock(&ostor->lock);
	ret = safe_write(ch->fd, data, dlen);
	ochunk_release(ostor, ch);
done:
	fast_log_ostor(fb, FLOS_OCHUNK_WRITE, cid, 0, ret, dlen);
	return ret;
}

int32_t ostor_read(struct ostor *ostor, struct fast_log_buf *fb, uint64_t cid,
		uint64_t off, char *data, uint32_t dlen)
{
	int ret;
	struct ochunk *ch;

	if (cid == RF_INVAL_CID) {
		ret = -EINVAL;
		goto done;
	}
	pthread_mutex_lock(&ostor->lock);
	ch = ostor_get_ochunk(ostor, fb, cid, 0);
	if (IS_ERR(ch)) {
		pthread_mutex_unlock(&ostor->lock);
		ret = FORCE_NEGATIVE(PTR_ERR(ch));
		goto done;
	}
	RB_REMOVE(ochunks_by_atime, &ostor->atime_head, ch);
	ch->refcnt++;
	pthread_mutex_unlock(&ostor->lock);
	ret = safe_pread(ch->fd, data, dlen, off);
	ochunk_release(ostor, ch);
done:
	if (ret < 0)
		fast_log_ostor(fb, FLOS_OCHUNK_READ, cid, off, ret, dlen);
	else
		fast_log_ostor(fb, FLOS_OCHUNK_READ, cid, off, 0, ret);
	return ret;
}

int ostor_unlink(struct ostor *ostor, struct fast_log_buf *fb, uint64_t cid)
{
	int ret, res;
	struct ochunk *ch;
	char path[PATH_MAX];

	if (cid == RF_INVAL_CID) {
		ret = -EINVAL;
		goto done;
	}
	/* Wait for the reference count to go to 0 before unlinking and freeing
	 * the chunk.  The lock/unlock calls invoke memory barriers, making the
	 * other threads' changes to ch->refcnt visible to us. */
	while (1) {
		pthread_mutex_lock(&ostor->lock);
		ch = ostor_get_ochunk(ostor, fb, cid, 0);
		if (IS_ERR(ch)) {
			pthread_mutex_unlock(&ostor->lock);
			ret = FORCE_NEGATIVE(PTR_ERR(ch));
			goto done;
		}
		if (ch->refcnt == -1) {
			pthread_mutex_unlock(&ostor->lock);
			ret = -ENOENT;
			goto done;
		}
		if (ch->refcnt == 0)
			break;
		pthread_mutex_unlock(&ostor->lock);
		mt_msleep(1);
	}
	ch->refcnt = -1;
	RB_REMOVE(ochunks_by_atime, &ostor->atime_head, ch);
	pthread_mutex_unlock(&ostor->lock);
	ochunk_get_path(ostor, path, sizeof(path), ch->cid);
	RETRY_ON_EINTR(res, unlink(path));
	if (res) {
		glitch_log("ostor error: failed to unlink %s: error %d\n",
			path, res);
	}
	pthread_mutex_lock(&ostor->lock);
	/* Now that the backing file has been deleted, we can evict the chunk
	 * from memory.  We couldn't do this earlier because then someone else
	 * might re-create the chunk.  His open() would then race with our
	 * unlink() operation. */
	ochunk_evict(ostor, fb, ch);
	pthread_mutex_unlock(&ostor->lock);
	ret = 0;
done:
	fast_log_ostor(fb, FLOS_OCHUNK_UNLINK, cid, 0, ret, 0);
	return ret;
}

static struct ochunk *ostor_get_ochunk(struct ostor *ostor,
		struct fast_log_buf *fb, uint64_t cid, int create)
{
	int ret;
	time_t cur_time;
	struct ochunk exemplar, *ch;

	memset(&exemplar, 0, sizeof(exemplar));
	exemplar.cid = cid;
	cur_time = mt_time();
	while (1) {
		if (ostor->shutdown) {
			ch = ERR_PTR(ESHUTDOWN);
			break;
		}
		ch = RB_FIND(ochunks_by_cid, &ostor->cid_head, &exemplar);
		if (ch) {
			if (ch->refcnt != -1) {
				/* The chunk exists in memory and is ready to use */
				break;
			}
			if (!create) {
				/* We're trying to read from something
				 * which either doesn't exist yet, or is
				 * in the process of being destroyed.
				 * ENOENT. */
				ch = ERR_PTR(ENOENT);
				break;
			}
			/* Busy-wait until the chunk is either fully
			 * created or fully destroyed.  Busy-waiting
			 * sucks, but we should not hit this case very
			 * often. */
			pthread_mutex_unlock(&ostor->lock);
			fast_log_ostor(fb, FLOS_OCHUNK_WAIT, cid,
				0, -EBUSY, 0);
			mt_msleep(1);
			pthread_mutex_lock(&ostor->lock);
			continue;
		}
		if (ostor->num_open < ostor->max_open) {
			ch = ochunk_alloc(ostor, cid);
			if (IS_ERR(ch))
				break;
			pthread_mutex_unlock(&ostor->lock);
			ret = ochunk_open(ostor, ch, create);
			fast_log_ostor(fb, FLOS_OCHUNK_ALLOC, cid,
				0, ret, ch->fd);
			pthread_mutex_lock(&ostor->lock);
			if (ret) {
				ochunk_evict(ostor, fb, ch);
				ch = ERR_PTR(ret);
				break;
			}
			/* The ochunk is now ready to use. */
			ch->refcnt = 0;
			break;
		}
		fast_log_ostor(fb, FLOS_OCHUNK_WAIT, cid, 0, -EMFILE,
			ostor->num_open);
		ostor->need_lru++;
		pthread_cond_signal(&ostor->lru_cond);
		pthread_cond_wait(&ostor->alloc_cond, &ostor->lock);
	}
	return ch;
}

/************************** lru *******************************/
static struct ochunk *ostor_find_dispoable_chunk(struct ostor *ostor,
			time_t cur_time)
{
	struct ochunk *ch;

	ch = RB_MIN(ochunks_by_atime, &ostor->atime_head);
	while (ch) {
		/* We can't remove a chunk that someone is using */
		if (ch->refcnt != 0) {
			ch = RB_NEXT(ochunks_by_atime, &ostor->atime_head, ch);
			continue;
		}
		/* If we're not desperate to evict something, make sure the
		 * chunk is old enough to evict. */
		if ((ostor->need_lru == 0) &&
				((ch->atime + ostor->atime_timeo) > cur_time))
			return NULL;
		return ch;
	}
	return NULL;
}

static int ostor_lru_thread(struct redfish_thread *rt)
{
	int res;
	struct timespec ts;
	struct ostor *ostor = rt->priv;
	struct ochunk *ch;

	pthread_mutex_lock(&ostor->lock);
	while (1) {
		if (ostor->shutdown) {
			pthread_mutex_unlock(&ostor->lock);
			return 0;
		}
		res = clock_gettime(CLOCK_MONOTONIC, &ts);
		if (res)
			abort();
		ch = ostor_find_dispoable_chunk(ostor, ts.tv_sec);
		if (!ch) {
			if (ostor->need_lru == 0)
				timespec_add_sec(&ts, OSTOR_LRU_LONG_PERIOD_SEC);
			else
				timespec_add_nsec(&ts, OSTOR_LRU_SHORT_PERIOD_NSEC);
			fast_log_ostor(rt->fb, FLOS_LRU_SLEEP, 0, 0,
				0, ostor->need_lru);
			pthread_cond_timedwait(&ostor->lru_cond,
					&ostor->lock, &ts);
			fast_log_ostor(rt->fb, FLOS_LRU_WAKE, 0, 0,
				0, ostor->need_lru);
			continue;
		}
		ch->refcnt = -1;
		RB_REMOVE(ochunks_by_atime, &ostor->atime_head, ch);
		ochunk_evict(ostor, rt->fb, ch);
	}
}
