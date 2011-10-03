/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "util/error.h"
#include "util/platform/pipe2.h"
#include "util/run_cmd.h"
#include "util/safe_io.h"
#include "util/string.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CVEC 50

int run_cmd(const char *cmd, ...)
{
	int res, pid, ret, i = 0;
	const char *c, *cvec[MAX_CVEC];
	va_list ap;
	va_start(ap, cmd);
	c = cmd;

	do {
		if (i == MAX_CVEC - 1)
			return -EDOM;
		cvec[i++] = c;
		c = va_arg(ap, const char*);
	} while (c != NULL);
	va_end(ap);
	cvec[i++] = NULL;

	pid = fork();
	if (pid == -1) {
		ret = errno;
		return ret;
	}
	else if (pid == 0) {
		int null_fd = open("/dev/null", O_WRONLY);
		if (null_fd < 0)
			_exit(127);
		RETRY_ON_EINTR(res, dup2(null_fd, STDERR_FILENO));
		RETRY_ON_EINTR(res, dup2(null_fd, STDOUT_FILENO));
		execvp(cmd, (char**)cvec);
		_exit(127);
	}
	return do_waitpid(pid);
}

int do_waitpid(int pid)
{
	int ret, status;
	RETRY_ON_EINTR(ret, waitpid(pid, &status, 0));
	if (ret < 0) {
		return RUN_CMD_INTERNAL_ERROR;
	}
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	else if (WIFSIGNALED(status)) {
		return RUN_CMD_EXITED_ON_SIGNAL;
	}
	/* terminated by unknown mechanism */
	return RUN_CMD_INTERNAL_ERROR;
}

int run_cmd_get_output(char *out, int out_len, const char **cvec)
{
	int res, pid, ret, pipefd[2] = { -1, -1 };
	char *o;
	int o_len;

	/* We can't accept buffers that are too small or too big.  */
	if ((out_len < 2) || (out == NULL)) {
		ret = -EDOM;
		goto done;
	}
	if (out_len > 512) {
		/* This is related to PIPE_BUF; see below for a
		 * full explanation. */
		ret = -EDOM;
		goto done;
	}
	/* Create a pipe with both ends set to nonblocking */
	ret = do_pipe2(pipefd, O_NONBLOCK);
	if (ret) {
		goto done;
	}
	pid = fork();
	if (pid == -1) {
		ret = errno;
		goto done;
	}
	else if (pid == 0) {
		RETRY_ON_EINTR(res, close(pipefd[PIPE_READ]));
		RETRY_ON_EINTR(res, dup2(pipefd[PIPE_WRITE], STDERR_FILENO));
		RETRY_ON_EINTR(res, dup2(pipefd[PIPE_WRITE], STDOUT_FILENO));
		execvp(cvec[0], (char**)cvec);
		_exit(127);
	}
	RETRY_ON_EINTR(res, close(pipefd[PIPE_WRITE]));
	pipefd[PIPE_WRITE] = -1;

	/* We block, waiting for the child process to finish.
	 *
	 * But wait! There is nobody reading from the pipe!
	 * Won't the child process block forever? No, it won't, because the
	 * pipe is nonblocking. However, if the child process writes more than
	 * PIPE_BUF bytes, we will lose the extra bytes at the end. That's ok,
	 * because we really just want to get a short status message.
	 * If you want to read more from the child command, you'll have to use
	 * a different function.

	 * POSIX guarantees that PIPE_BUF is at least 512 bytes. That is why we
	 * reject out_len sizes of more than 512.
	 */
	ret = do_waitpid(pid);
	memset(out, 0, out_len);
	o_len = out_len - 1;
	o = out;
	while (o_len > 0) {
		int res = read(pipefd[PIPE_READ], o, o_len);
		if (res <= 0) {
			if (res == 0)
				break;
			ret = errno;
			if ((ret == EAGAIN) || (ret == EWOULDBLOCK))
				break;
			if (ret == EINTR)
				continue;
			else {
				snprintf(out, out_len, "Pipe read error: %d", ret);
				goto done;
			}
		}
		o += res;
		o_len -= res;
	}
done:
	if (pipefd[PIPE_READ] != -1)
		RETRY_ON_EINTR(res, close(pipefd[PIPE_READ]));
	if (pipefd[PIPE_WRITE] != -1)
		RETRY_ON_EINTR(res, close(pipefd[PIPE_WRITE]));
	return ret;
}

int start_cmd_give_input(const char **cvec, int *pid)
{
	int mypid, res, ret, pipefd[2] = { -1, -1 };
	*pid = -1;

	/* Create a pipe to pass in data */
	ret = do_pipe2(pipefd, O_CLOEXEC);
	if (ret) {
		return FORCE_NEGATIVE(ret);
	}
	mypid = fork();
	if (mypid == -1) {
		return FORCE_NEGATIVE(errno);
	}
	else if (mypid == 0) {
		RETRY_ON_EINTR(res, close(pipefd[PIPE_WRITE]));
		RETRY_ON_EINTR(res, dup2(pipefd[PIPE_READ], STDIN_FILENO));
		execvp(cvec[0], (char**)cvec);
		_exit(127);
	}
	*pid = mypid;
	RETRY_ON_EINTR(res, close(pipefd[PIPE_READ]));
	pipefd[PIPE_READ] = -1;

	return pipefd[PIPE_WRITE];
}

int get_colocated_path(const char *argv0, const char *other,
			   char *path, size_t path_len)
{
	int ret;
	char *slash;
	char wd[PATH_MAX];
	char argv0d[PATH_MAX];

	if (argv0[0] == '/') {
		/* absolute path */
		wd[0] = '\0';
	}
	else {
		/* relative path */
		memset(wd, 0, sizeof(wd));
		if (getcwd(wd, sizeof(wd) - 1) == NULL)
			return -ENAMETOOLONG;
	}
	ret = zsnprintf(argv0d, sizeof(argv0d), "%s", argv0);
	if (ret)
		return ret;
	slash = rindex(argv0d, '/');
	if (slash == NULL)
		argv0d[0] = '\0';
	else
		slash[1] = '\0';
	ret = zsnprintf(path, path_len, "%s/%s/%s", wd, argv0d, other);
	if (ret)
		return -ENAMETOOLONG;
	return 0;
}

int shell_escape(const char *src, char *dst, size_t dst_len)
{
	int ret = 0;
	size_t i = 0;
	if (dst_len == 0)
		return 0;
	if (i + 1 >= dst_len) {
		ret = -ENAMETOOLONG;
		goto done;
	}
	dst[i++] = '\'';
	while (1) {
		char c = *src++;
		if (c == '\0')
			break;
		switch (c) {
		case '\'':
			if (i + 4 >= dst_len) {
				ret = -ENAMETOOLONG;
				goto done;
			}
			dst[i++] = '\'';
			dst[i++] = '\\';
			dst[i++] = '\'';
			dst[i++] = '\'';
			break;
		case '\\':
			if (i + 2 >= dst_len) {
				ret = -ENAMETOOLONG;
				goto done;
			}
			dst[i++] = '\\';
			dst[i++] = '\\';
			break;
		default:
			if (i + 1 >= dst_len) {
				ret = -ENAMETOOLONG;
				goto done;
			}
			dst[i++] = c;
			break;
		}
	}
	if (i + 1 >= dst_len) {
		ret = -ENAMETOOLONG;
		goto done;
	}
	dst[i++] = '\'';
done:
	dst[i++] = '\0';
	return ret;
}
