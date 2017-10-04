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

static int gif2bmp(FILE *input, FILE *output);
static void usage(void);
static int args_parse(int argc, char * const argv[],
	char **s_input, char **s_output);
static int io_open(char *s_input, char *s_output, FILE **f_input, FILE **f_output);
static void io_close(FILE *f_input, FILE *f_output);

static int gif2bmp(FILE *input, FILE *output)
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
char **s_input, char **s_output)
{
	int chr;

	opterr = 0; /* disable error messages by getopt() */
	while ((chr = getopt(argc, argv, "i:o:h")) != -1) {
		switch (chr) {
		case 'i':
			*s_input = optarg;
			break;
		case 'o':
			*s_output = optarg;
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

static int io_open(char *s_input, char *s_output, FILE **f_input, FILE **f_output)
{
	if (s_input != NULL) {
		*f_input = fopen(s_input, "rb");
		if (!*f_input) {
			fprintf(stderr, "Error: opening file '%s': %s\n",
				s_input, strerror(errno));
			return 1;
		}
	}
	else
		*f_input = stdin;

	if (s_output != NULL) {
		*f_output = fopen(s_output, "wb");
		if (!*f_output) {
			fprintf(stderr, "Error: opening file '%s': %s\n",
				s_output, strerror(errno));
			if (*f_input != stdin)
				fclose(*f_input);
			return 1;
		}
	}
	else
		*f_output = stdout;

	return 0;
}

static void io_close(FILE *f_input, FILE *f_output)
{
	if (f_input != stdin)
		fclose (f_input);

	if (f_output != stdout)
		fclose(f_output);
}

int main(int argc, char *argv[])
{
	char *s_input = NULL;
	char *s_output = NULL;
	FILE *f_input = NULL;
	FILE *f_output = NULL;
	int ret;

	if (args_parse(argc, argv, &s_input, &s_output))
		return 1;

	if (io_open(s_input, s_output, &f_input, &f_output))
		return 1;

	ret = gif2bmp(f_input, f_output);
	io_close(f_input, f_output);

	return ret;
}

