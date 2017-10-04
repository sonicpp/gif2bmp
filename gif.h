/*
 * gif.h - Read GIF image from f_gif and save it into p_img
 *
 * Copyright (C) 2017 Jan Havran
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef GIF_H
#define GIF_H

#include "gif2bmp.h"

extern size_t gif_load(image_t *p_img, FILE *f_gif);

#endif // GIF_H

