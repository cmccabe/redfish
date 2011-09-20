/*
 * The OneFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "jorm/json.h"
#include "util/simple_io.h"

#include <errno.h>
#include <json/json_object_private.h> /* need for struct json_object_iter */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

enum remove_comments_state
{
	REMOVE_COMMENTS_STATE_NORMAL,
	REMOVE_COMMENTS_STATE_IN_STR,
	REMOVE_COMMENTS_STATE_IN_COMMENT,
};

static void remove_comments(char *b)
{
	enum remove_comments_state state = REMOVE_COMMENTS_STATE_NORMAL;
	while (1) {
		char c = *b;
		if (c == '\0')
			return;
		switch (state) {
		case REMOVE_COMMENTS_STATE_NORMAL:
			if (c == '"')
				state = REMOVE_COMMENTS_STATE_IN_STR;
			else if (c == '#') {
				*b = ' ';
				state = REMOVE_COMMENTS_STATE_IN_COMMENT;
			}
			break;
		case REMOVE_COMMENTS_STATE_IN_STR:
			if (c == '"')
				state = REMOVE_COMMENTS_STATE_NORMAL;
			break;
		case REMOVE_COMMENTS_STATE_IN_COMMENT:
			if (c == '\n')
				state = REMOVE_COMMENTS_STATE_NORMAL;
			else
				*b = ' ';
			break;
		default:
			abort();
			break;
		}
		++b;
	}
}

struct json_object* parse_json_file(const char *file_name,
				char *err, size_t err_len)
{
	char *buf;
	struct stat stbuf;
	ssize_t res;
	int ret;
	struct json_object* jret;

	if (stat(file_name, &stbuf) < 0) {
		ret = errno;
		snprintf(err, err_len, "can't stat '%s': error %d",
			 file_name, ret);
		return NULL;
	}
	if (stbuf.st_size > MAX_JSON_FILE_SZ) {
		snprintf(err, err_len, "file '%s' is too big at '%lld "
			 "bytes. The maximum JSON file we can open is %lld "
			 "bytes.\n", file_name, (long long int)stbuf.st_size,
			 (long long int)MAX_JSON_FILE_SZ);
		return NULL;
	}
	buf = malloc(stbuf.st_size + 1);
	buf[stbuf.st_size] = '\0';
	res = simple_io_read_whole_file(file_name, buf, (int)stbuf.st_size);
	if (res < 0) {
		snprintf(err, err_len, "failed to read '%s': error %Zd\n",
			 file_name, res);
		free(buf);
		return NULL;
	}
	remove_comments(buf);
	jret = parse_json_string(buf, err, err_len);
	if (!jret) {
		free(buf);
		return NULL;
	}
	free(buf);
	return jret;
}

static int offset_to_line_num(const char *str, int offset)
{
	int i = 0, line_num = 1;
	while ((i < offset) && str[i]) {
		if (str[i] == '\n')
			++line_num;
		++i;
	}
	return line_num;
}

struct json_object* parse_json_string(const char *str,
			char *err, size_t err_len)
{
	struct json_object *ret = NULL;
	struct json_tokener* jtok = json_tokener_new();
	if (!jtok)
		goto done;
	ret = json_tokener_parse_ex(jtok, str, strlen(str));
	if (!ret) {
		snprintf(err, err_len, "malformed JSON: error '%s' at offset %d "
			"(line %d)\n", json_tokener_errors[jtok->err],
			jtok->char_offset,
			offset_to_line_num(str, jtok->char_offset));
		goto done_free_json_tokener;
	}
done_free_json_tokener:
	json_tokener_free(jtok);
done:
	return ret;
}

const char *json_ty_to_str(enum json_type ty)
{
	switch (ty) {
	case json_type_null:
		return "null";
	case json_type_boolean:
		return "boolean";
	case json_type_double:
		return "double";
	case json_type_int:
		return "int";
	case json_type_object:
		return "object";
	case json_type_array:
		return "array";
	case json_type_string:
		return "string";
	default:
		return "unknown";
	}
}
