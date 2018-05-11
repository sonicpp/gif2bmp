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
	uint8_t gct_size : 3;
	uint8_t sort_flag : 1;
	uint8_t col_resolution : 3;
	uint8_t gct_flag : 1;
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

/* Packed Fields for Image Descriptor */
struct GIF_img_desc_field
{
	uint8_t lct_size : 3;
	uint8_t reserved : 2;
	uint8_t sort_flag : 1;
	uint8_t interlace_flag : 1;
	uint8_t lct_flag : 1;
} __attribute__((packed));

/* Image Descriptor */
struct GIF_img_desc
{
	uint16_t left_edge;
	uint16_t top_edge;
	uint16_t width;
	uint16_t height;
	struct GIF_img_desc_field field;
} __attribute__((packed));

/* Color Table */
struct GIF_ct
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
} __attribute__((packed));

/* Packed Fields for Graphic Control Extension */
struct GIF_ext_gcontrol_field
{
	uint8_t reserved : 3;
	uint8_t disposal : 3;
	uint8_t input_flag : 1;
	uint8_t transparet_flag : 1;
} __attribute__((packed));

/* Graphic Control Extension */
struct GIF_ext_gcontrol
{
	struct GIF_ext_gcontrol_field field;
	uint16_t delay;
	uint8_t transparent;
} __attribute__((packed));

/* Plain Text Extension */
struct GIF_ext_plain
{
	uint16_t grid_left;
	uint16_t grid_top;
	uint16_t grid_width;
	uint16_t grid_height;
	uint8_t cell_width;
	uint8_t cell_height;
	uint8_t foreground;
	uint8_t background;
} __attribute__((packed));

/* Application Extension */
struct GIF_ext_app
{
	char identifier[8];
	uint8_t auth[3];
} __attribute__((packed));

#define SIZE_HEADER		(sizeof(struct GIF_header))
#define SIZE_LSD		(sizeof(struct GIF_lsd))
#define SIZE_IMG_DESC		(sizeof(struct GIF_img_desc))
#define SIZE_EXT_GCONTROL	(sizeof(struct GIF_ext_gcontrol))
#define SIZE_EXT_PLAIN		(sizeof(struct GIF_ext_plain))
#define SIZE_EXT_APP		(sizeof(struct GIF_ext_app))

#define BLOCK_TERM		((uint8_t)  0x00)

#define COLOR_TABLE_SIZE(size)	(3u * (1u << ((size) + 1u)))

#define INTRO_EXTENSION		((uint8_t) 0x21)
#define INTRO_IMG_DESC		((uint8_t) 0x2C)
#define TRAILER			((uint8_t) 0x3B)

#define EXT_GCONTROL		((uint8_t) 0xF9)
#define EXT_COMMENT		((uint8_t) 0xFE)
#define EXT_PLAIN_TXT		((uint8_t) 0x01)
#define EXT_APP			((uint8_t) 0xFF)

#define GIF_ERROR(string) \
	do { \
		fprintf(stderr, string); \
		gif_len = 0; \
		goto gif_err; \
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
	if (memcmp(header->version, "89a", 3) &&
	memcmp(header->version, "87a", 3))
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

static size_t load_ext_gcontrol(struct GIF_ext_gcontrol *ext, FILE *f_gif)
{
	assert(ext);
	assert(f_gif);
	size_t cnt;
	uint8_t byte;

	cnt = fread(&byte, 1, 1, f_gif);
	if (cnt != 1 || byte != SIZE_EXT_GCONTROL)
		return 0;

	cnt = fread(ext, 1, SIZE_EXT_GCONTROL, f_gif);
	if (cnt != SIZE_EXT_GCONTROL)
		return 0;

	cnt = fread(&byte, 1, 1, f_gif);
	if (cnt != 1 || byte != BLOCK_TERM)
		return 0;

	return 1 + SIZE_EXT_GCONTROL + 1;
}

static size_t load_ext_comment(FILE *f_gif)
{
	assert(f_gif);
	char comment[256];
	size_t cnt, ret;
	uint8_t byte;

	/* Read block size */
	cnt = fread(&byte, 1, 1, f_gif);
	if (cnt != 1)
		return 0;
	ret = 1;

	/* Read all data sub-blocks */
	while (byte != BLOCK_TERM) {
		cnt = fread(comment, 1, byte, f_gif);
		if (cnt != byte)
			return 0;
		ret += cnt;

		comment[byte] = '\0';
		printf("GIF: Comment: '%s'\n", comment);

		/* Read block size */
		cnt = fread(&byte, 1, 1, f_gif);
		if (cnt != 1)
			return 0;
		ret++;
	}

	return ret;
}

static size_t load_ext_plain(struct GIF_ext_plain *ext, FILE *f_gif)
{
	assert(ext);
	assert(f_gif);
	uint8_t data[256];
	size_t cnt, ret;
	uint8_t byte;

	cnt = fread(&byte, 1, 1, f_gif);
	if (cnt != 1 || byte != SIZE_EXT_PLAIN)
		return 0;
	ret = 1;

	cnt = fread(&ext, 1, SIZE_EXT_PLAIN, f_gif);
	if (cnt != SIZE_EXT_PLAIN)
		return 0;
	ret += SIZE_EXT_PLAIN;

	cnt = fread(&byte, 1, 1, f_gif);
	if (cnt != 1 || byte != BLOCK_TERM)
		return 0;
	ret++;

	/* Read all data sub-blocks */
	while (byte != BLOCK_TERM) {
		cnt = fread(data, 1, byte, f_gif);
		if (cnt != byte)
			return 0;
		ret += cnt;

		/* Read block size */
		cnt = fread(&byte, 1, 1, f_gif);
		if (cnt != 1)
			return 0;
		ret++;
	}

	return ret;
}

static size_t load_ext_app(struct GIF_ext_app *ext, FILE *f_gif)
{
	assert(ext);
	assert(f_gif);
	uint8_t data[256];
	size_t cnt, ret;
	uint8_t byte;
	char identifier[9];

	cnt = fread(&byte, 1, 1, f_gif);
	if (cnt != 1 || byte != SIZE_EXT_APP)
		return 0;
	ret = 1;

	cnt = fread(ext, 1, SIZE_EXT_APP, f_gif);
	if (cnt != SIZE_EXT_APP)
		return 0;
	ret += SIZE_EXT_APP;

	memcpy(identifier, ext->identifier, 8);
	identifier[8] = '\0';
	printf("GIF: Application Identifier: %s\n", identifier);

	/* TODO do-while */
	cnt = fread(&byte, 1, 1, f_gif);
	if (cnt != 1 || byte != BLOCK_TERM)
		return 0;
	ret++;

	/* Read all data sub-blocks */
	while (byte != BLOCK_TERM) {
		cnt = fread(data, 1, byte, f_gif);
		if (cnt != byte)
			return 0;
		ret += cnt;

		/* Read block size */
		cnt = fread(&byte, 1, 1, f_gif);
		if (cnt != 1)
			return 0;
		ret++;
	}

	return ret;
}

static size_t load_ext(FILE *f_gif)
{
	assert(f_gif);
	struct GIF_ext_gcontrol gcontrol;
	struct GIF_ext_plain plain;
	struct GIF_ext_app app;
	size_t cnt;
	uint8_t byte;

	/* Identify the current extension */
	cnt = fread(&byte, 1, 1, f_gif);
	if (cnt != 1)
		return 0;

	switch (byte) {
	case EXT_GCONTROL:
		cnt = load_ext_gcontrol(&gcontrol, f_gif);
		break;
	case EXT_COMMENT:
		cnt = load_ext_comment(f_gif);
		break;
	case EXT_PLAIN_TXT:
		cnt = load_ext_plain(&plain, f_gif);
		break;
	case EXT_APP:
		cnt = load_ext_app(&app, f_gif);
		break;
	default:
		return 0;
	}

	if (cnt == 0)
		return 0;

	return cnt + 1; /* extension identifier + extension itself */
}

static size_t load_img_desc(struct GIF_img_desc *desc, FILE *f_gif)
{
	assert(desc);
	assert(f_gif);
	size_t cnt;

	cnt = fread(desc, 1, SIZE_IMG_DESC, f_gif);
	if (cnt != SIZE_IMG_DESC)
		return 0;

	return cnt;
}

static size_t load_image(image_t *img, uint16_t col_table_size,
	const struct GIF_ct *col_table, FILE *f_gif)
{
	assert(img);
	assert(col_table);
	assert(f_gif);

	return 0;
}

size_t gif_load(image_t *p_img, FILE *f_gif)
{
	struct GIF_header header;
	struct GIF_lsd lsd;
	struct GIF_img_desc img_desc;
	struct GIF_ct *cct = NULL;	/* current color table */
	struct GIF_ct *gct = NULL;	/* global color table */
	struct GIF_ct *lct = NULL;	/* local color table */
	size_t gif_len = 0;
	size_t block_len = 0;
	uint16_t cct_size = 0;		/* current color table size */
	uint16_t gct_size = 0;		/* global color table size */
	uint16_t lct_size = 0;		/* local color table size */
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
		gct_size = COLOR_TABLE_SIZE(lsd.field.gct_size);
	}

	/* Check label - determine which block follows */
	if (fread(&byte, 1, 1, f_gif) == 0)
		GIF_ERROR("GIF: missing file content\n");
	gif_len++;

	/* Parse Data Streams */
	do {
		/* Parse extensions - if present */
		while (byte == INTRO_EXTENSION) {
			if ((block_len = load_ext(f_gif)) == 0)
				GIF_ERROR("GIF: invalid extension\n");
			gif_len += block_len;

			if (fread(&byte, 1, 1, f_gif) == 0)
				GIF_ERROR("GIF: missing file content\n");
			gif_len++;
		}

		/* Parse Image Descriptor */
		if (byte != INTRO_IMG_DESC)
			GIF_ERROR("GIF: missing image description\n");
		if ((block_len = load_img_desc(&img_desc, f_gif)) == 0)
			GIF_ERROR("GIF: invalid image descriptor\n");
		gif_len += block_len;

		/* Check if Image and Screen descriptor size differs */
		if (lsd.width != img_desc.width ||
			lsd.height != img_desc.height)
			GIF_ERROR("Image and Screen desc size differs\n");

		/* Parse Local Color Table - if present */
		if (img_desc.field.lct_flag) {
			if ((lct = (struct GIF_ct *) malloc(
			COLOR_TABLE_SIZE(img_desc.field.lct_size))) == NULL)
				GIF_ERROR("Not enough memory\n");

			if ((block_len = load_color_table(lct,
			COLOR_TABLE_SIZE(img_desc.field.lct_size), f_gif)) == 0)
				GIF_ERROR("Invalid Local Color Table\n");
			gif_len += block_len;
			lct_size = COLOR_TABLE_SIZE(img_desc.field.lct_size);
		}

		/* Alloc canvas for image */
		if ((p_img->data =
		(uint8_t *) malloc(lsd.width * lsd.height * 3u)) == NULL)
			GIF_ERROR("Not enough memory\n");
		p_img->width  = lsd.width;
		p_img->height = lsd.height;

		/* Choose Current Color Table */
		cct = (lct) ? lct : gct;
		cct_size = (lct) ? lct_size : gct_size;

		/* Parse image data */
		block_len = load_image(p_img, cct_size, cct, f_gif);
		gif_len += block_len;

		if (lct) {
			free(lct);
			lct = NULL;
			lct_size = 0;
		}
		byte = TRAILER;
	} while (byte != TRAILER);

	/* TODO: parse remaining bytes? */

	goto gif_end;

gif_err:
	gif_len = 0;
	p_img->width = p_img->height = 0;
	if (p_img->data) {
		free(p_img->data);
		p_img->data = NULL;
	}
gif_end:
	if (gct)
		free(gct);
	if (lct)
		free(lct);

	return gif_len;
}

