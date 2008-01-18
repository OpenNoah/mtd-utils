/*
 * Copyright (c) International Business Machines Corp., 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * An utility to create UBI volumes.
 *
 * Authors: Artem Bityutskiy <dedekind@infradead.org>
 *          Frank Haverkamp <haver@vnet.ibm.com>
 */

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libubi.h>
#include "common.h"

#define PROGRAM_VERSION "1.6"
#define PROGRAM_NAME    "ubimkvol"

/* The variables below are set by command line arguments */
struct args {
	int vol_id;
	int vol_type;
	long long bytes;
	int lebs;
	int alignment;
	const char *name;
	int nlen;
	const char *node;
	int maxavs;
};

static struct args args = {
	.vol_type = UBI_DYNAMIC_VOLUME,
	.bytes = -1,
	.lebs = -1,
	.alignment = 1,
	.vol_id = UBI_VOL_NUM_AUTO,
	.name = NULL,
	.nlen = 0,
	.node = NULL,
	.maxavs = 0,
};

static const char *doc = PROGRAM_NAME " version " PROGRAM_VERSION
			 " - a tool to create UBI volumes.";

static const char *optionsstr =
"-a, --alignment=<alignment>   volume alignment (default is 1)\n"
"-n, --vol_id=<volume ID>      UBI volume ID, if not specified, the volume ID\n"
"                              will be assigned automatically\n"
"-N, --name=<name>             volume name\n"
"-s, --size=<bytes>            volume size volume size in bytes, kilobytes (KiB)\n"
"                              or megabytes (MiB)\n"
"-S, --lebs=<LEBs count>       alternative way to give volume size in logical\n"
"                              eraseblocks\n"
"-m, --maxavsize               set volume size to maximum available size\n"
"-t, --type=<static|dynamic>   volume type (dynamic, static), default is dynamic\n"
"-h, --help                    print help message\n"
"-V, --version                 print program version";

static const char *usage =
"Usage: " PROGRAM_NAME " <UBI device node file name> [-h] [-a <alignment>] [-n <volume ID>] [-N <name>]\n"
"\t\t\t[-s <bytes>] [-S <LEBs>] [-t <static|dynamic>] [-V] [-m]\n"
"\t\t\t[--alignment=<alignment>][--vol_id=<volume ID>] [--name=<name>]\n"
"\t\t\t[--size=<bytes>] [--lebs=<LEBs>] [--type=<static|dynamic>] [--help]\n"
"\t\t\t[--version] [--maxavsize]\n\n"
"Example: " PROGRAM_NAME "/dev/ubi0 -s 20MiB -N config_data - create a 20 Megabytes volume\n"
"         named \"config_data\" on UBI device /dev/ubi0.";

static const struct option long_options[] = {
	{ .name = "alignment", .has_arg = 1, .flag = NULL, .val = 'a' },
	{ .name = "vol_id",    .has_arg = 1, .flag = NULL, .val = 'n' },
	{ .name = "name",      .has_arg = 1, .flag = NULL, .val = 'N' },
	{ .name = "size",      .has_arg = 1, .flag = NULL, .val = 's' },
	{ .name = "lebs",      .has_arg = 1, .flag = NULL, .val = 'S' },
	{ .name = "type",      .has_arg = 1, .flag = NULL, .val = 't' },
	{ .name = "help",      .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",   .has_arg = 0, .flag = NULL, .val = 'V' },
	{ .name = "maxavsize", .has_arg = 0, .flag = NULL, .val = 'm' },
	{ NULL, 0, NULL, 0},
};

static int param_sanity_check(void)
{
	int len;

	if (args.bytes == -1 && !args.maxavs && args.lebs == -1) {
		errmsg("volume size was not specified (use -h for help)");
		return -1;
	}

	if ((args.bytes != -1 && (args.maxavs || args.lebs != -1))  ||
	    (args.lebs != -1  && (args.maxavs || args.bytes != -1)) ||
	    (args.maxavs && (args.bytes != -1 || args.lebs != -1))) {
		errmsg("size specified with more then one option");
		return -1;
	}

	if (args.name == NULL) {
		errmsg("volume name was not specified (use -h for help)");
		return -1;
	}

	len = strlen(args.name);
	if (len > UBI_MAX_VOLUME_NAME) {
		errmsg("too long name (%d symbols), max is %d",
			len, UBI_MAX_VOLUME_NAME);
		return -1;
	}

	return 0;
}

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key;
		char *endp;

		key = getopt_long(argc, argv, "a:n:N:s:S:t:hVm", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 't':
			if (!strcmp(optarg, "dynamic"))
				args.vol_type = UBI_DYNAMIC_VOLUME;
			else if (!strcmp(optarg, "static"))
				args.vol_type = UBI_STATIC_VOLUME;
			else {
				errmsg("bad volume type: \"%s\"", optarg);
				return -1;
			}
			break;

		case 's':
			args.bytes = strtoull(optarg, &endp, 0);
			if (endp == optarg || args.bytes <= 0) {
				errmsg("bad volume size: \"%s\"", optarg);
				return -1;
			}
			if (*endp != '\0') {
				int mult = ubiutils_get_multiplier(endp);

				if (mult == -1) {
					errmsg("bad size specifier: \"%s\" - "
					       "should be 'KiB', 'MiB' or 'GiB'", endp);
					return -1;
				}
				args.bytes *= mult;
			}
			break;

		case 'S':
			args.lebs = strtoull(optarg, &endp, 0);
			if (endp == optarg || args.lebs <= 0 || *endp != '\0') {
				errmsg("bad volume size: \"%s\"", optarg);
				return -1;
			}
			break;

		case 'a':
			args.alignment = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || args.alignment <= 0) {
				errmsg("bad volume alignment: \"%s\"", optarg);
				return -1;
			}
			break;

		case 'n':
			args.vol_id = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || args.vol_id < 0) {
				errmsg("bad volume ID: " "\"%s\"", optarg);
				return -1;
			}
			break;

		case 'N':
			args.name = optarg;
			args.nlen = strlen(args.name);
			break;

		case 'h':
			fprintf(stderr, "%s\n\n", doc);
			fprintf(stderr, "%s\n\n", usage);
			fprintf(stderr, "%s\n", optionsstr);
			exit(EXIT_SUCCESS);

		case 'V':
			fprintf(stderr, "%s\n", PROGRAM_VERSION);
			exit(EXIT_SUCCESS);

		case 'm':
			args.maxavs = 1;
			break;

		case ':':
			errmsg("parameter is missing");
			return -1;

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}

	if (optind == argc) {
		errmsg("UBI device name was not specified (use -h for help)");
		return -1;
	} else if (optind != argc - 1) {
		errmsg("more then one UBI device specified (use -h for help)");
		return -1;
	}

	args.node = argv[optind];

	if (param_sanity_check())
		return -1;

	return 0;
}

int main(int argc, char * const argv[])
{
	int err;
	libubi_t libubi;
	struct ubi_dev_info dev_info;
	struct ubi_vol_info vol_info;
	struct ubi_mkvol_request req;

	err = parse_opt(argc, argv);
	if (err)
		return err;

	libubi = libubi_open();
	if (!libubi) {
		errmsg("cannot open libubi");
		perror("libubi_open");
		return -1;
	}

	err = ubi_node_type(libubi, args.node);
	if (err == 2) {
		errmsg("\"%s\" is an UBI volume node, not an UBI device node",
		       args.node);
		goto out_libubi;
	} else if (err < 0) {
		errmsg("\"%s\" is not an UBI device node", args.node);
		goto out_libubi;
	}

	err = ubi_get_dev_info(libubi, args.node, &dev_info);
	if (err) {
		errmsg("cannot get information about UBI device \"%s\"",
		       args.node);
		perror("ubi_get_dev_info");
		goto out_libubi;
	}

	if (args.maxavs) {
		args.bytes = dev_info.avail_bytes;
		printf("Set volume size to %lld\n", req.bytes);
	}

	if (args.lebs != -1) {
		args.bytes = dev_info.leb_size;
		args.bytes -= dev_info.leb_size % args.alignment;
		args.bytes *= args.lebs;
	}

	req.vol_id = args.vol_id;
	req.alignment = args.alignment;
	req.bytes = args.bytes;
	req.vol_type = args.vol_type;
	req.name = args.name;

	err = ubi_mkvol(libubi, args.node, &req);
	if (err < 0) {
		errmsg("cannot UBI create volume");
		perror("ubi_mkvol");
		goto out_libubi;
	}

	args.vol_id = req.vol_id;

	/* Print information about the created device */
	err = ubi_get_vol_info1(libubi, dev_info.dev_num, args.vol_id, &vol_info);
	if (err) {
		errmsg("cannot get information about newly created UBI volume");
		perror("ubi_get_vol_info1");
		goto out_libubi;
	}

	printf("Volume ID %d, size %d LEBs (", vol_info.vol_id, vol_info.rsvd_lebs);
	ubiutils_print_bytes(vol_info.rsvd_bytes, 0);
	printf("), LEB size ");
	ubiutils_print_bytes(vol_info.leb_size, 1);
	printf(", %s volume, name \"%s\", alignment %d\n",
	       req.vol_type == UBI_DYNAMIC_VOLUME ? "dynamic" : "static",
	       vol_info.name, vol_info.alignment);

	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}
