/*
 * bmp.c - Convert p_img to BMP format and write it to f_bmp
 *
 * Copyright (C) 2017 Jan Havran
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bmp.h"

/* BMP header */
struct BMP_header
{
	char signature[2];
	uint32_t size;
	uint16_t reserved1;
	uint16_t reserved2;
	uint32_t offset;
} __attribute__((packed));

/* DIB header (version BITMAPINFOHEADER) */
struct DIB_header
{
	uint32_t head_size;
	uint32_t width;
	uint32_t height;
	uint16_t planes;
	uint16_t bpp;
	uint32_t compression;
	uint32_t img_size;
	uint32_t h_res;		/* pixels per meter */
	uint32_t v_res;		/* pixels per meter */
	uint32_t colors;	/* number of used colors */
	uint32_t high_colors;	/* number of important colors */
} __attribute__((packed));

#define SIZE_BMP_HEADER		(sizeof(struct BMP_header))
#define SIZE_DIB_HEADER		(sizeof(struct DIB_header))
#define SIZE_ROW_PADDING(w)	(((w) % 4 == 0) ? (w) : ((w) + 4 - (w) % 4))

static void set_bmp_header(struct BMP_header *header, const image_t *img)
{
	assert(header);
	assert(img);

	header->signature[0] = 'B';
	header->signature[1] = 'M';
	header->size = SIZE_BMP_HEADER + SIZE_DIB_HEADER
		+ SIZE_ROW_PADDING(img->width) * img->height;
	header->reserved1 = 0;
	header->reserved2 = 0;
	header->offset = SIZE_BMP_HEADER + SIZE_DIB_HEADER;
}

static void set_dip_header(struct DIB_header *header, const image_t *img)
{
	assert(header);
	assert(img);

	header->head_size = SIZE_DIB_HEADER;
	header->width = img->width;
	header->height = img->height;
	header->planes = 1;
	header->bpp = 24;
	header->compression = 0;
	header->img_size = SIZE_ROW_PADDING(img->width) * img->height;
	header->h_res = 2835;
	header->v_res = 2835;
	header->colors = 0;
	header->high_colors = 0;
}

size_t bmp_save(const image_t *p_img, FILE *f_bmp)
{
	struct BMP_header bmp;
	struct DIB_header dip;
	size_t bmp_len = 0;
	size_t ret = 0;
	size_t cnt;
	uint16_t rows;
	uint8_t *row_data = NULL;

	row_data = (uint8_t *) malloc(
		SIZE_ROW_PADDING(p_img->width * 3) * sizeof(uint8_t));
	if (row_data == NULL) {
		fprintf(stderr, "Not enough memory\n");
		goto bmp_err;
	}

	/* Fill and write BMP header */
	set_bmp_header(&bmp, p_img);
	cnt = fwrite(&bmp, 1, SIZE_BMP_HEADER, f_bmp);
	if (cnt != SIZE_BMP_HEADER) {
		fprintf(stderr, "Write error\n");
		goto bmp_err;
	}
	bmp_len += cnt;

	/* Fill and write DIP header */
	set_dip_header(&dip, p_img);
	cnt = fwrite(&dip, 1, SIZE_DIB_HEADER, f_bmp);
	if (cnt != SIZE_DIB_HEADER) {
		fprintf(stderr, "Write error\n");
		goto bmp_err;
	}
	bmp_len += cnt;

	/* Start storing rows upside-down */
	for (rows = p_img->height - 1; rows < p_img->height; rows--) {
		/* BMP uses BGR color model */
		for (int i = 0; i < p_img->width; i++) {
			/* Blue */
			row_data[i * 3 + 0] = p_img->data[
				rows * (p_img->width * 3) + i * 3 + 2];
			/* Green */
			row_data[i * 3 + 1] = p_img->data[
				rows * (p_img->width  * 3) + i * 3 + 1];
			/* Red */
			row_data[i * 3 + 2] = p_img->data[
				rows * (p_img->width  * 3) + i * 3 + 0];
		}
		/* Clear the rest of the row */
		memset(row_data + p_img->width * 3, 0,
			SIZE_ROW_PADDING(p_img->width * 3) - p_img->width * 3);

		/* Write row */
		cnt = fwrite(row_data, 1, SIZE_ROW_PADDING(p_img->width * 3),
			f_bmp);
		if (cnt != SIZE_ROW_PADDING(p_img->width * 3)) {
			fprintf(stderr, "Write error\n");
			goto bmp_err;
		}
		bmp_len += cnt;
	}

	ret = bmp_len;
bmp_err:
	free(row_data);

	return ret;
}

