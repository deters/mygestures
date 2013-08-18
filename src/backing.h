/* backing.h - save, extend, and restore contents of root window

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

#ifndef BACKING_H
#define BACKING_H

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#define BACKING_INC 100

struct backing
{
    Display *dpy;
    Window root;
    GC gc;

    Pixmap root_pixmap;
    Picture root_pict;
    XRenderPictFormat *root_format;

    Pixmap brush_pixmap;
    Picture brush_pict;
    XRenderPictFormat *brush_format;

    int total_width, total_height, depth;

    int active;

    int x, y;
    int width, height;
};
typedef struct backing backing_t;

int backing_init(backing_t *backing, Display *dpy, Window root, int width, int height, int depth);
void backing_deinit(backing_t *backing);
int backing_save(backing_t *backing, int x, int y);
int backing_restore(backing_t *backing);
int backing_reconfigure(backing_t *backing, int width, int height, int depth);

#endif
