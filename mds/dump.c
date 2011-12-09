/*
 * The RedFish distributed filesystem
 *
 * Copyright 2011, Colin Patrick McCabe <cmccabe@alumni.cmu.edu>
 *
 * This is licensed under the Apache License, Version 2.0.  See file COPYING.
 */

#include "common/config/mstorc.h"
#include "core/glitch_log.h"
#include "core/process_ctx.h"
#include "jorm/json.h"
#include "mds/mstor.h"
#include "mds/net.h"
#include "util/error.h"
#include "util/str_to_int.h"
#include "util/string.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(int exitstatus)
{
	static const char *usage_lines[] = {
"fishmdump: dumps the contents of a RedFish metadata file",
"See http://www.club.cc.cmu.edu/~cmccabe/redfish.html for the most up-to-date",
"information about RedFish.",
"",
"usage:",
"fishmdump [options] <file-name>",
"",
"options:",
"-h",
"    Show this help message",
"-o",
"    Output file [default: stdout]",
NULL
	};
	print_lines(stderr, usage_lines);
	exit(exitstatus);
}

static void parse_argv(int argc, char **argv, const char **mstor_path,
		const char **ofile)
{
	int c;

	while ((c = getopt(argc, argv, "ho:")) != -1) {
		switch (c) {
		case 'h':
			usage(EXIT_SUCCESS);
			break;
		case 'o':
			*ofile = optarg;
			break;
		case '?':
			glitch_log("error parsing options.\n\n");
			usage(EXIT_FAILURE);
		}
	}
	if (optind == argc) {
		glitch_log("You must supply the name of a RedFish "
			"metadata file.  Type -h for help.\n");
		usage(EXIT_FAILURE);
	}
	if (optind + 1 != argc) {
		glitch_log("Junk at end of commandline.  Type -h for help.\n");
		usage(EXIT_FAILURE);
	}
	*mstor_path = argv[optind];
}

int run(const char *mstor_path, FILE *ofp)
{
	int ret;
	struct mstor* mstor = NULL;
	struct mstorc *conf = NULL;
	
	conf = JORM_INIT_mstorc();
	if (!conf) {
		ret = -ENOMEM;
		goto done;
	}
	conf->mstor_path = strdup(mstor_path);
	if (!conf->mstor_path) {
		ret = -ENOMEM;
		goto done;
	}
	conf->mstor_cache_size = 1024;
	mstor = mstor_init(g_fast_log_mgr, conf);
	if (IS_ERR(mstor)) {
		ret = PTR_ERR(mstor);
		mstor = NULL;
		goto done;
	}
	ret = mstor_dump(mstor, ofp);
	if (ret) {
		goto done;
	}
	ret = 0;

done:
	if (mstor)
		mstor_shutdown(mstor);
	if (conf)
		JORM_FREE_mstorc(conf);
	return ret;
}

int main(int argc, char **argv)
{
	int ret;
	const char *mstor_path = NULL;
	const char *ofile = NULL;
	FILE *ofp = NULL;

	parse_argv(argc, argv, &mstor_path, &ofile);
	if (utility_ctx_init(argv[0]))
		return EXIT_FAILURE;
	if (!ofile) {
		ofp = stdout;
	}
	else {
		ofp = fopen(ofile, "w");
		if (!ofp) {
			ret = -errno;
			glitch_log("error opening file '%s' for writing: "
				   "error %d\n", ofile, ret);
			goto done;
		}
	}
	ret = run(mstor_path, ofp);
	if (ofile) {
		if (fclose(ofp)) {
			ret = -errno;
			glitch_log("error closing file '%s': "
				   "error %d\n", ofile, ret);
			goto done;
		}
	}
	ret = 0;
done:
	process_ctx_shutdown();
	return ret;
}
