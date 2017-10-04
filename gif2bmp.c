/*
 * gif2bmp.c - Read GIF image, convert it to PNG image and write it to output
 *
 * Copyright (C) 2017 Jan Havran
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

#include "gif2bmp.h"

static void usage(void);
static int args_parse(int argc, char * const argv[],
	char **i_name, char **o_name);
static int io_open(char *i_name, char *o_name, FILE **i_file, FILE **o_file);
static void io_close(FILE *i_file, FILE *o_file);

int gif2bmp(FILE *input, FILE *output)
{
	return 0;
}

static void usage(void)
{
	printf("gif2bmp usage:\n" \
		"-i\tinput GIF file\n" \
		"-o\toutput BMP file\n" \
		"-h\tdisplay this help and exit\n");
}

static int args_parse(int argc, char * const argv[],
char **i_name, char **o_name)
{
	int chr;

	opterr = 0; /* disable error messages by getopt() */
	while ((chr = getopt(argc, argv, "i:o:h")) != -1) {
		switch (chr) {
		case 'i':
			*i_name = optarg;
			break;
		case 'o':
			*o_name = optarg;
			break;
		case 'h':
		case '?':
			usage();
			return 1;
		}
	}

	if (optind < argc) {
		usage();
		return 1;
	}

	return 0;
}

static int io_open(char *i_name, char *o_name, FILE **i_file, FILE **o_file)
{
	if (i_name != NULL) {
		*i_file = fopen(i_name, "rb");
		if (!*i_file) {
			fprintf(stderr, "Error: opening file '%s': %s\n",
				i_name, strerror(errno));
			return 1;
		}
	}
	else
		*i_file = stdin;

	if (o_name != NULL) {
		*o_file = fopen(o_name, "wb");
		if (!*o_file) {
			fprintf(stderr, "Error: opening file '%s': %s\n",
				o_name, strerror(errno));
			if (*i_file != stdin)
				fclose(*i_file);
			return 1;
		}
	}
	else
		*o_file = stdout;

	return 0;
}

static void io_close(FILE *i_file, FILE *o_file)
{
	if (i_file != stdin)
		fclose (i_file);

	if (o_file != stdout)
		fclose(o_file);
}

int main(int argc, char *argv[])
{
	char *i_name = NULL;
	char *o_name = NULL;
	FILE *i_file = NULL;
	FILE *o_file = NULL;
	int ret;

	if (args_parse(argc, argv, &i_name, &o_name))
		return 1;

	if (io_open(i_name, o_name, &i_file, &o_file))
		return 1;

	ret =  gif2bmp(i_file, o_file);
	io_close(i_file, o_file);

	return ret;
}

