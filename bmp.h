/*
 * bmp.h - Convert p_img to PNG format and write it to f_gif
 *
 * Copyright (C) 2017 Jan Havran
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef BMP_H
#define BMP_H

#include "gif2bmp.h"

extern size_t bmp_save(image_t *p_img, FILE *f_gif);

#endif // BMP_H

