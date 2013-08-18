/* backing.c - save, extend, and restore contents of root window

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

#include <stdio.h>
#include <X11/Xlib.h>

#include "backing.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

int backing_init(backing_t *backing, Display *dpy, Window root, int width, int height, int depth)
{
    XRenderPictFormat templ;
    int screen = DefaultScreen(dpy);
    unsigned long gcm;
    XGCValues gcv;

    backing->dpy = dpy;
    backing->root = root;
    backing->active = 0;

    gcm = 0;
    gcm |= GCSubwindowMode; gcv.subwindow_mode = IncludeInferiors;
    backing->gc = XCreateGC(backing->dpy, backing->root, gcm, &gcv);

    backing->root_pixmap = 0;
    backing->brush_pixmap = 0;
    backing->total_width = width;
    backing->total_height = height;
    backing->depth = depth;
    backing->root_pict = 0;
    backing->brush_pict = 0;

    backing->root_format = XRenderFindVisualFormat(dpy,DefaultVisual(dpy, screen));

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
    backing->brush_format = XRenderFindFormat (dpy,
					       PictFormatType|
					       PictFormatDepth|
					       PictFormatAlpha|
					       PictFormatAlphaMask|
					       PictFormatRed|
					       PictFormatRedMask|
					       PictFormatGreen|
					       PictFormatGreenMask|
					       PictFormatBlue|
					       PictFormatBlueMask,
					       &templ, 0);

    return 0;
}

void backing_deinit(backing_t *backing)
{
    backing->active = 0;
    XFreeGC(backing->dpy, backing->gc);
    if (backing->root_pixmap) {
	XFreePixmap(backing->dpy, backing->root_pixmap);
	backing->root_pixmap = 0;

	XRenderFreePicture(backing->dpy, backing->root_pict);
	backing->root_pict = 0;
    }
    if (backing->brush_pixmap) {
	XFreePixmap(backing->dpy, backing->brush_pixmap);
	backing->brush_pixmap = 0;

	XRenderFreePicture(backing->dpy, backing->brush_pict);
	backing->brush_pict = 0;
    }
}

int backing_save(backing_t *backing, int x, int y)
{
    if (backing->active == 0) {
	backing_reconfigure(backing,
			    backing->total_width,
			    backing->total_height,
			    backing->depth);
	backing->active = 1;

	backing->x = BACKING_INC * (x / BACKING_INC);
	backing->y = BACKING_INC * (y / BACKING_INC);
	backing->width = BACKING_INC;
	backing->height = BACKING_INC;
	XCopyArea(backing->dpy, backing->root,
		  backing->root_pixmap, backing->gc,
		  backing->x, backing->y,
		  backing->width, backing->height,
		  backing->x, backing->y);
    } else {
	if (x >= backing->x + backing->width) {
	    int new_max_x, width_inc;
	    new_max_x = BACKING_INC * ((x / BACKING_INC) + 1);
	    width_inc = new_max_x - (backing->x + backing->width);
	    XCopyArea(backing->dpy, backing->root,
		      backing->root_pixmap, backing->gc,
		      backing->x + backing->width, backing->y,
		      width_inc, backing->height,
		      backing->x + backing->width, backing->y);
	    backing->width += width_inc;
	}
	if (x < backing->x) {
	    int new_x, width_inc;
	    new_x = BACKING_INC * (x / BACKING_INC);
	    width_inc = backing->x - new_x;
	    XCopyArea(backing->dpy, backing->root,
		      backing->root_pixmap, backing->gc,
		      new_x, backing->y,
		      width_inc, backing->height,
		      new_x, backing->y);
	    backing->width += width_inc;
	    backing->x = new_x;
	}
	if (y >= backing->y + backing->height) {
	    int new_max_y, height_inc;
	    new_max_y = BACKING_INC * ((y / BACKING_INC) + 1);
	    height_inc = new_max_y - (backing->y + backing->height);
	    XCopyArea(backing->dpy, backing->root,
		      backing->root_pixmap, backing->gc,
		      backing->x, backing->y + backing->height,
		      backing->width, height_inc,
		      backing->x, backing->y + backing->height);
	    backing->height += height_inc;
	}
	if (y < backing->y) {
	    int new_y, height_inc;
	    new_y = BACKING_INC * (y / BACKING_INC);
	    height_inc = backing->y - new_y;
	    XCopyArea(backing->dpy, backing->root,
		      backing->root_pixmap, backing->gc,
		      backing->x, new_y,
		      backing->width, height_inc,
		      backing->x, new_y);
	    backing->height += height_inc;
	    backing->y = new_y;
	}
    }
    return 0;
}

int backing_restore(backing_t *backing)
{
    XCopyArea(backing->dpy, backing->root_pixmap,
	      backing->root, backing->gc,
	      backing->x, backing->y,
	      backing->width, backing->height,
	      backing->x, backing->y);
    backing->active = 0;

    XFreePixmap(backing->dpy, backing->root_pixmap);
    backing->root_pixmap = 0;
    XRenderFreePicture(backing->dpy, backing->root_pict);
    backing->root_pict = 0;

    XFreePixmap(backing->dpy, backing->brush_pixmap);
    backing->brush_pixmap = 0;
    XRenderFreePicture(backing->dpy, backing->brush_pict);
    backing->brush_pict = 0;

    return 0;
}

int backing_reconfigure(backing_t *backing, int width, int height, int depth)
{
    XRenderColor color;
    XRenderPictureAttributes attr;
    backing_t old_backing = *backing;

    backing->total_width = width;
    backing->total_height = height;
    backing->depth = depth;

    backing->root_pixmap = XCreatePixmap(backing->dpy, backing->root,
					 backing->total_width,
					 backing->total_height,
					 depth);
    attr.subwindow_mode = IncludeInferiors;
    backing->root_pict = XRenderCreatePicture(backing->dpy,
					      backing->root,
					      backing->root_format,
					      CPSubwindowMode,
					      &attr);

    backing->brush_pixmap = XCreatePixmap(backing->dpy, backing->root,
					  backing->total_width,
					  backing->total_height,
					  32);
    backing->brush_pict = XRenderCreatePicture(backing->dpy,
					       backing->brush_pixmap,
					       backing->brush_format,
					       0, 0);

    color.red = 0;
    color.green = 0;
    color.blue = 0;
    color.alpha = 0;
    XRenderFillRectangle(backing->dpy,
			 PictOpSrc, backing->brush_pict, &color,
			 0, 0, backing->total_width, backing->total_height);

    if (old_backing.root_pixmap) {
	if (old_backing.depth == depth) {
	    XCopyArea(backing->dpy, old_backing.root_pixmap,
		      backing->root_pixmap, backing->gc,
		      old_backing.x, old_backing.y,
		      old_backing.width, old_backing.height,
		      old_backing.x, old_backing.y);
	}
	XFreePixmap(old_backing.dpy, old_backing.root_pixmap);
	old_backing.root_pixmap = 0;
	XRenderFreePicture(old_backing.dpy, old_backing.root_pict);
	old_backing.root_pict = 0;
    }
    if (old_backing.brush_pixmap) {
	XRenderComposite(backing->dpy,
			 PictOpSrc,
			 old_backing.brush_pict, None, backing->brush_pict,
			 0, 0, 0, 0, 0, 0,
			 old_backing.total_width, old_backing.total_height);
	XFreePixmap(old_backing.dpy, old_backing.brush_pixmap);
	old_backing.brush_pixmap = 0;
	XRenderFreePicture(old_backing.dpy, old_backing.brush_pict);
	old_backing.brush_pict = 0;
    }

    return 0;
}

