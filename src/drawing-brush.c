/* brush.c - draw a translucent brush

 Copyright 2001 Carl Worth

 Many thanks to Keith Packard for the Render extension, (especially
 the new Disjoint and Conjoint operators that make this drop-shadow
 rendering possible). Keith Packard also wrote basically all of the
 code in this file.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#include "drawing-bresenham.h"
#include "drawing-brush.h"
#define const
#include "drawing-brush-image.h"
#include "drawing-brush-shadow.h"
#undef const

#ifdef DMALLOC
#include "dmalloc.h"
#endif

static void fix_image(unsigned char *image, int npixels);

int brush_init(brush_t *brush, backing_t *backing) {
	Display *dpy = backing->dpy;
	Window root = backing->root;
	int screen = DefaultScreen(backing->dpy);
	XRenderPictFormat templ;

	XImage *image;
	XRenderPictFormat *image_format;
	GC image_gc;

	brush->dpy = backing->dpy;
	brush->backing = backing;

	brush->image_width = brush_image->width;
	brush->image_height = brush_image->height;
	brush->image_pict = 0;

	brush->shadow_width = brush_shadow.width;
	brush->shadow_height = brush_shadow.height;
	brush->shadow_pict = 0;

	brush->last_x = 0;
	brush->last_y = 0;

	brush->image_pixmap = XCreatePixmap(dpy, root, brush->image_width,
			brush->image_height, 32);

	brush->shadow_pixmap = XCreatePixmap(dpy, root, brush->shadow_width,
			brush->shadow_height, 32);

	/*
	 * Create brush/shadow pixmaps and initialize with brush/shadow images
	 */
	templ.type = PictTypeDirect;
	templ.depth = 32;
	templ.direct.alpha = 24;
	templ.direct.alphaMask = 0xff;
	templ.direct.red = 16;
	templ.direct.redMask = 0xff;
	templ.direct.green = 8;
	templ.direct.greenMask = 0xff;
	templ.direct.blue = 0;
	templ.direct.blueMask = 0xff;
	image_format = XRenderFindFormat(dpy,
	PictFormatType |
	PictFormatDepth |
	PictFormatAlpha |
	PictFormatAlphaMask |
	PictFormatRed |
	PictFormatRedMask |
	PictFormatGreen |
	PictFormatGreenMask |
	PictFormatBlue |
	PictFormatBlueMask, &templ, 0);

	brush->image_pict = XRenderCreatePicture(dpy, brush->image_pixmap,
			image_format, 0, 0);

	brush->shadow_pict = XRenderCreatePicture(dpy, brush->shadow_pixmap,
			image_format, 0, 0);

	image_gc = XCreateGC(dpy, brush->image_pixmap, 0, 0);

	fix_image(brush_image->pixel_data,
			brush_image->width * brush_image->height);

	image = XCreateImage(dpy, DefaultVisual(dpy, screen), 32, ZPixmap, 0,
			(char *) brush_image->pixel_data, brush->image_width,
			brush->image_height, 32, brush->image_width * 4);
	XPutImage(dpy, brush->image_pixmap, image_gc, image, 0, 0, 0, 0,
			brush->image_width, brush->image_height);

	fix_image(brush_shadow.pixel_data,
			brush_shadow.width * brush_shadow.height);

	image = XCreateImage(dpy, DefaultVisual(dpy, screen), 32, ZPixmap, 0,
			(char *) brush_shadow.pixel_data, brush->shadow_width,
			brush->shadow_height, 32, brush->shadow_width * 4);
	XPutImage(dpy, brush->shadow_pixmap, image_gc, image, 0, 0, 0, 0,
			brush->shadow_width, brush->shadow_height);

	XFreeGC(dpy, image_gc);

	return 0;
}

void brush_deinit(brush_t *brush) {
	XFreePixmap(brush->dpy, brush->image_pixmap);
	XFreePixmap(brush->dpy, brush->shadow_pixmap);
	XRenderFreePicture(brush->dpy, brush->image_pict);
	XRenderFreePicture(brush->dpy, brush->shadow_pict);
}

void brush_draw(brush_t *brush, int x, int y) {
	XRenderComposite(brush->dpy,
	PictOpConjointOverReverse, brush->shadow_pict, None,
			brush->backing->brush_pict, 0, 0, 0, 0, x, y, brush->shadow_width,
			brush->shadow_height);

	XRenderComposite(brush->dpy,
	PictOpConjointOver, brush->image_pict, None, brush->backing->brush_pict, 0,
			0, 0, 0, x, y, brush->image_width, brush->image_height);

	XCopyArea(brush->dpy, brush->backing->root_pixmap,
			DefaultRootWindow(brush->dpy), brush->backing->gc, x, y,
			brush->image_width, brush->image_height, x, y);

	XRenderComposite(brush->dpy,
	PictOpOver, brush->backing->brush_pict, None, brush->backing->root_pict, x,
			y, 0, 0, x, y, brush->image_width, brush->image_height);

	brush->last_x = x;
	brush->last_y = y;
}

void brush_line_to(brush_t *brush, int x, int y) {
	bresenham_skip_first((bresenham_cb_t) brush_draw, brush, brush->last_x,
			brush->last_y, x, y);
	brush->last_x = x;
	brush->last_y = y;
}

/* Fix byte order as it comes from the GIMP, and pre-multiply
 alpha.
 Thanks to Keith Packard.
 */
static void fix_image(unsigned char *image, int npixels) {
#define FbIntMult(a,b,t) ( (t) = (a) * (b) + 0x80, ( ( ( (t)>>8 ) + (t) )>>8 ) )

	int i;
	for (i = 0; i < npixels; i++) {
		unsigned char a, r, g, b;
		unsigned short t;

		b = image[i * 4 + 0];
		g = image[i * 4 + 1];
		r = image[i * 4 + 2];
		a = image[i * 4 + 3];

		r = FbIntMult(a, r, t);
		g = FbIntMult(a, g, t);
		b = FbIntMult(a, b, t);
		image[i * 4 + 0] = r;
		image[i * 4 + 1] = g;
		image[i * 4 + 2] = b;
		image[i * 4 + 3] = a;
	}
}
