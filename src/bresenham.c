/* bresenham.c - compute line from x1,y1 to x2,y2 using Bresham's algorithm

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

#include "bresenham.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

void bresenham(bresenham_cb_t cb, void *data, int x1, int y1, int x2, int y2)
{
    (cb)(data, x1, y1);
    bresenham_skip_first_last(cb, data, x1, y1, x2, y2);
    if (x2 != x1 || y2 != y1)
	(cb)(data, x2, y2);
}

void bresenham_skip_first(bresenham_cb_t cb, void *data,
			  int x1, int y1, int x2, int y2)
{
    bresenham_skip_first_last(cb, data, x1, y1, x2, y2);
    if (x2 != x1 || y2 != y1)
	(cb)(data, x2, y2);
}

void bresenham_skip_last(bresenham_cb_t cb, void *data,
			 int x1, int y1, int x2, int y2)
{
    (cb)(data, x1, y1);
    bresenham_skip_first_last(cb, data, x1, y1, x2, y2);
}

void bresenham_skip_first_last(bresenham_cb_t cb, void *data,
			       int x1, int y1, int x2, int y2)
{
    int dy = y2 - y1;
    int dx = x2 - x1;
    int stepx, stepy;
    
    if (dy < 0) {
	dy = -dy;
	stepy = -1;
    } else {
	stepy = 1;
    }
    if (dx < 0) {
	dx = -dx;
	stepx = -1;
    } else {
	stepx = 1;
    }

    if (dx == 0 && dy == 0)
	return;

    dy <<= 1;
    dx <<= 1;
    if (dx > dy) {
	int fraction = dy - (dx >> 1);
	while (1) {
	    if (fraction >= 0) {
		y1 += stepy;
		fraction -= dx;
	    }
	    x1 += stepx;
	    fraction += dy;
	    if (x1 == x2)
		break;
	    (cb)(data, x1, y1);
	}
    } else {
	int fraction = dx - (dy >> 1);
	while (1) {
	    if (fraction >= 0) {
		x1 += stepx;
		fraction -= dy;
	    }
	    y1 += stepy;
	    fraction += dx;
	    if (y1 == y2)
		break;
	    (cb)(data, x1, y1);
	}
    }
}
