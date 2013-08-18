/* brush.h - draw a translucent brush

   Copyright 2001 Carl Worth

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/
#ifndef BRUSH_H
#define BRUSH_H

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#include "backing.h"

struct brush
{
    Display *dpy;
    backing_t *backing;

    int image_width;
    int image_height;
    Pixmap image_pixmap;
    Picture image_pict;

    int shadow_width;
    int shadow_height;
    Pixmap shadow_pixmap;
    Picture shadow_pict;

    int last_x;
    int last_y;
};
typedef struct brush brush_t;

int brush_init(brush_t *brush, backing_t *backing);
void brush_deinit(brush_t *brush);

void brush_draw(brush_t *brush, int x, int y);
void brush_line_to(brush_t *brush, int x, int y);

#endif
