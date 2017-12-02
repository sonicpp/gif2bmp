/*
 * gif.c - Read GIF image from f_gif and save it into p_img
 *
 * Copyright (C) 2017 Jan Havran
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "gif.h"

/* GIF header */
struct GIF_header
{
	uint8_t signature[3];
	uint8_t version[3];
} __attribute__((packed));

/* Packed Fields for Logical Screen Descriptor */
struct GIF_lsd_field
{
	uint8_t gct_flag : 1;
	uint8_t col_resolution : 3;
	uint8_t sort_flag : 1;
	uint8_t gct_size : 3;
} __attribute__((packed));

/* Logical Screen Descriptor */
struct GIF_lsd
{
	uint16_t width;
	uint16_t height;
	struct GIF_lsd_field field;
	uint8_t transID;
	uint8_t aspect;
} __attribute__((packed));

/* Color Table */
struct GIF_ct
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
} __attribute__((packed));

#define SIZE_HEADER		(sizeof(struct GIF_header))
#define SIZE_LSD		(sizeof(struct GIF_lsd))

#define COLOR_TABLE_SIZE(size)	(3u * (1u << ((size) + 1u)))

#define INTRO_EXTENSION		((uint8_t) 0x21)
#define INTRO_IMG_DESC		((uint8_t) 0x2C)
#define TRAILER			((uint8_t) 0x3B)

#define GIF_ERROR(string) \
	do { \
		fprintf(stderr, string); \
		gif_len = 0; \
		goto gif_end; \
	} while(0)

static size_t load_header(struct GIF_header *header, FILE *f_gif)
{
	assert(header);
	assert(f_gif);
	size_t cnt;

	cnt = fread(header, 1, SIZE_HEADER, f_gif);
	if (cnt != SIZE_HEADER)
		return 0;

	if (memcmp(header->signature, "GIF", 3))
		return 0;
	if (memcmp(header->version, "89a", 3) && memcmp(header->version, "87a", 3))
		return 0;

	return cnt;
}

static size_t load_lsd(struct GIF_lsd *lsd, FILE *f_gif)
{
	assert(lsd);
	assert(f_gif);
	size_t cnt;

	cnt = fread(lsd, 1, SIZE_LSD, f_gif);
	if (cnt != SIZE_LSD)
		cnt = 0;

	return cnt;
}

static size_t load_color_table(struct GIF_ct *table, uint16_t size, FILE *f_gif)
{
	assert(table);
	assert(f_gif);
	assert(size % sizeof(struct GIF_ct) == 0);
	size_t cnt;

	cnt = fread(table, 1, size, f_gif);
	if (cnt != size)
		cnt = 0;

	return cnt;
}

static size_t load_ext(FILE *f_gif)
{
	/*TODO */
	return 0;
}

size_t gif_load(image_t *p_img, FILE *f_gif)
{
	struct GIF_header header;
	struct GIF_lsd lsd;
	struct GIF_ct *gct = NULL;	/* global color table */
	struct GIF_ct *lct = NULL;	/* local color table */
	size_t gif_len = 0;
	size_t block_len = 0;
	uint8_t byte;

	/* Parse Header */
	if ((block_len = load_header(&header, f_gif)) == 0)
		GIF_ERROR("GIF: Invalid header\n");
	gif_len += block_len;

	/* Parse Logical Screen Descriptor */
	if ((block_len = load_lsd(&lsd, f_gif)) == 0)
		GIF_ERROR("GIF: Invalid Local Screen Descriptor\n");
	gif_len += block_len;

	/* Parse Global Color Table - if present */
	if (lsd.field.gct_flag) {
		if ((gct = (struct GIF_ct *) malloc(
			COLOR_TABLE_SIZE(lsd.field.gct_size))) == NULL) {
			GIF_ERROR("Not enough memory\n");
		}
		if ((block_len = load_color_table(gct,
			COLOR_TABLE_SIZE(lsd.field.gct_size), f_gif)) == 0) {
			GIF_ERROR("GIF: Invalid Global Color Table\n");
		}
		gif_len += block_len;
	}

	/* Parse Data Streams */
	do {
		/* Check label - determine which block follows */
		if (fread(&byte, 1, 1, f_gif) == 0)
			GIF_ERROR("GIF: missing file content\n");
		gif_len++;

		/* Parse extensions - if present */
		while (byte == INTRO_EXTENSION) {
			if ((block_len = load_ext(f_gif)) == 0)
				GIF_ERROR("GIF: invalid extension\n");
			gif_len += block_len;

			if (fread(&byte, 1, 1, f_gif) == 0)
				GIF_ERROR("GIF: missing file content\n");
			gif_len++;
		}

		if (byte != INTRO_IMG_DESC)
			GIF_ERROR("GIF: missing image description\n");
		/* TODO: parse image description */

		if (lct) {
			free(lct);
			lct = NULL;
		}
	} while (byte != TRAILER);

	/* TODO: parse remaining bytes? */

gif_end:
	if (gct)
		free(gct);
	if (lct)
		free(lct);

	return gif_len;
}

