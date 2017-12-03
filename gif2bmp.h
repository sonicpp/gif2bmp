/*
 * gif2bmp.c - Read GIF image, convert it to BMP image and write it to output
 *
 * Copyright (C) 2017 Jan Havran
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef GIF2BMP_H
#define GIF2BMP_H

#include <stdint.h>

typedef struct
{
	uint16_t width;
	uint16_t height;
	uint8_t *data;
} image_t;

#endif // GIF2BMP_H

