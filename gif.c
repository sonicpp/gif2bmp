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

/* LZW/image configuration */
typedef struct
{
	uint16_t min_code;
	uint16_t palette_size;
	uint16_t clear_code;
	uint16_t end_code;
	uint16_t start_code;
} lzw_info_t;

/* LZW table */
typedef struct
{
	uint16_t row;
	uint8_t val;
} table_t;

#define SIZE_HEADER		(sizeof(struct GIF_header))
#define SIZE_LSD		(sizeof(struct GIF_lsd))
#define SIZE_IMG_DESC		(sizeof(struct GIF_img_desc))
#define SIZE_EXT_GCONTROL	(sizeof(struct GIF_ext_gcontrol))
#define SIZE_EXT_PLAIN		(sizeof(struct GIF_ext_plain))
#define SIZE_EXT_APP		(sizeof(struct GIF_ext_app))

#define BLOCK_TERM		((uint8_t)  0x00)
#define BLOCK_EMPTY		((uint16_t) 0xFFFE)
#define BLOCK_ERR		((uint16_t) 0xFFFF)

#define TABLE_TERM		((uint16_t) 0xFFFF)
#define TABLE_MAX_WIDTH		(12u)

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

static size_t read_block(uint8_t *block, FILE *f_gif)
{
	size_t cnt;
	uint8_t len;

	/* Get the block length */
	cnt = fread(&len, 1, 1, f_gif);
	if (cnt != 1)
		return BLOCK_ERR;

	if (len == BLOCK_TERM)
		return BLOCK_TERM;

	cnt = fread(block, 1, len, f_gif);
	if (cnt != len)
		return BLOCK_ERR;

	return cnt;
}

uint16_t dict_get_row_len(table_t *table, uint16_t row)
{
	uint16_t len = 1;

	while (table[row].row != TABLE_TERM) {
		row = table[row].row;
		len++;
	}

	return len;
}

static uint16_t dict_get_val(table_t *table, uint16_t row, uint16_t index)
{
	uint16_t len = dict_get_row_len(table, row);
	uint16_t pos = 0;
	uint8_t val = 0;

	/* Get val from row[index] */
	while (pos < len - index) {
		val = table[row].val;
		row = table[row].row;
		pos += 1;
	}

	return val;
}

static uint16_t unpack_code(uint16_t block_len, uint16_t *block_inx,
	uint8_t *block, uint8_t *shift, uint8_t width, uint32_t *prev_stream,
	uint8_t *prev_cnt)
{
	static int clear = 1;	/* Indicator whether we needs data from previous
				   data block or not */
	uint32_t stream;	/* Current 4B of streamu */
	uint16_t code;		/* Loaded code */
	uint8_t b1, b2, b3, b4;	/* Bytes forming "stream" variable */
	uint16_t mask = ((uint16_t) 0xFFFF) >> (16 - width);
	uint16_t stream_req = *shift + width;	/* Required number of valid
						   bits in data stream */

	/* Completely whole block has been read*/
	if (*block_inx == block_len) {
		return BLOCK_EMPTY;
	}

	/* Join current byte with 3 upcomming bytes (if they are available)
	   into 32b long stream from which we will mask code of required
	   width */
	b1 = block[*block_inx];
	b2 = (*block_inx + 1 < block_len) ?
		block[*block_inx + 1] : (uint8_t) 0x00;
	b3 = (*block_inx + 2 < block_len) ?
		block[*block_inx + 2] : (uint8_t) 0x00;
	b4 = (*block_inx + 3 < block_len) ?
		block[*block_inx + 3] : (uint8_t) 0x00;
	stream = (((uint32_t) b1) << 0) |
		(((uint32_t) b2) << 8) |
		(((uint32_t) b3) << 16) |
		(((uint32_t) b4) << 24);

	/* Do we have enough bits? */
	if (stream_req > (block_len - *block_inx) * 8) {
		/* Store some data for upcoming data block */
		*prev_stream = stream;
		*prev_cnt = block_len - *block_inx;
		clear = 0;
		return BLOCK_EMPTY;
	}

	/* Join current stream with code from previous block if necessary */
	if (!clear) {
		if (*prev_cnt == 2) {
			stream = stream << 16;
			*prev_stream = *prev_stream & 0xFFFF;
			*block_inx = -2;
		}
		else {
			stream = stream << 8;
			*prev_stream = *prev_stream & 0xFF;
			*block_inx = -1;
		}
		stream = stream | *prev_stream;
	}

	/* Get code from stream */
	code = stream >> *shift;
	code = code & mask;

	*shift += width;

	/* Update shift position and current block index */
	while (*shift >= 8) {
		*shift = *shift - 8;
		*block_inx += 1;
	}

	clear = 1;
	return code;
}

static size_t decompress_data(image_t *img, uint16_t block_len, uint8_t *block,
	const struct GIF_ct *col_table, const lzw_info_t *lzw_info)
{
	/* Static variables */
	static uint32_t img_pos = 0;
	static table_t table[1u << TABLE_MAX_WIDTH];
	static uint16_t table_size = 0;
	static uint16_t prev = 0xFFFF;	/* Previous code */
	/* Static variables for fn 'unpack_code' */
	static uint8_t shift = 0;
	static uint8_t bits = 0;	/* Code width */
	static uint32_t prev_stream = 0;
	static uint8_t prev_cnt = 0;

	uint16_t table_size_max;
	uint16_t data_inx = 0;		/* Pos in data block */
	uint16_t entry_size;
	uint16_t code;
	uint8_t byte;

	/* Initialization of variables */
	if (img_pos == 0) {
		table_size = lzw_info->start_code;
		bits = lzw_info->min_code + 1;
		for (unsigned i = 0; i < lzw_info->palette_size; i++) {
			table[i].row = TABLE_TERM;
			table[i].val = i;
		}
	}

	/* Current maximum table size */
	table_size_max = (1 << bits) - 1;

	/* Read code by code from data block */
	while ((code = unpack_code(block_len, &data_inx, block, &shift,
		bits, &prev_stream, &prev_cnt)) != BLOCK_EMPTY) {
		/* Clear Code */
		if (code == lzw_info->clear_code) {
			table_size = lzw_info->start_code;
			bits = lzw_info->min_code + 1;
			table_size_max = (1 << bits) - 1;
		}
		/* End Code */
		else if (code == lzw_info->end_code) {
			img_pos = 0;
			return 1;
		}
		/* Always print first word after Clear Code */
		else if (prev == lzw_info->clear_code) {
			/* Store pixels */
			memcpy(img->data + img_pos, &col_table[code], 3);
			img_pos += 3;
		}
		/* Create new entry */
		else {
			table[table_size].row = prev;
			/* Create new entry by entry which is already in
			   the dictionary */
			if (code < table_size) {
				table[table_size].val =
					dict_get_val(table, code, 0);
			}
			/* Create new entry by entry which has been just
			   created by the compressor */
			else if (code == table_size) {
				table[table_size].val =
					dict_get_val(table, code, 0);
			}
			else {
				fprintf(stderr,
					"GIF: LZW key not in dictionary\n");
				return 1;
			}
			table_size += 1;

			/* Convert entry into pixels and store them */
			entry_size = dict_get_row_len(table, code);
			for (uint16_t i = 0; i < entry_size; i++) {
				byte = dict_get_val(table, code, i);
				memcpy(img->data + img_pos,
					&col_table[byte], 3);
				img_pos += 3;
			}
		}

		/* Extend table if necessary */
		if (table_size == table_size_max + 1) {
			/* Ignoring table overflow is non-standard behaviour
			   IMHO - but some images are compressed this way
			   (clear code stored too late) */
			if (bits < TABLE_MAX_WIDTH)
				bits++;
			table_size_max = (1 << bits) - 1;
		}

		prev = code;
	}

	return 0;
}

static size_t load_image(image_t *img, uint16_t col_table_size,
	const struct GIF_ct *col_table, FILE *f_gif)
{
	assert(img);
	assert(col_table);
	assert(f_gif);
	size_t cnt;
	size_t ret;
	lzw_info_t lzw_info;
	uint16_t block_len;
	uint8_t dict_width;
	uint8_t block[256];

	/* Read the init key width */
	cnt = fread(&dict_width, 1, 1, f_gif);
	if (cnt != 1) {
		fprintf(stderr, "GIF: LZW error\n");
		return 0;
	}
	ret = cnt;

	lzw_info.min_code = dict_width;
	lzw_info.palette_size = col_table_size / 3;
	lzw_info.clear_code = 1 << dict_width;
	lzw_info.end_code = lzw_info.clear_code + 1;
	lzw_info.start_code = lzw_info.end_code + 1;

	/* Read and parse image data blocks one by one */
	while (!((block_len = read_block(block, f_gif)) == BLOCK_ERR
		|| block_len == BLOCK_TERM)) {
		ret += block_len + 1;

		/* Parse data block */
		if (decompress_data(img, block_len, block, col_table, &lzw_info))
			break;
	}

	return (block_len == BLOCK_ERR) ? 0 : ret;
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
		if ((block_len = load_image(p_img, cct_size, cct, f_gif)) == 0)
			GIF_ERROR("GIF: Invalid picture data\n");
		gif_len += block_len;

		if (lct) {
			free(lct);
			lct = NULL;
			lct_size = 0;
		}

		/* Read empty block */
		do {
			if (fread(&byte, 1, 1, f_gif) == 0)
				GIF_ERROR("GIF: missing file content\n");
			gif_len++;
		} while (byte == 0);

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

