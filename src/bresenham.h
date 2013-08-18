/* bresenham.h - compute line from x1,y1 to x2,y2 using Bresham's algorithm

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

#ifndef BRESENHAM_H
#define BRESENHAM_H

typedef void (*bresenham_cb_t)(void *data, int x, int y);

void bresenham(bresenham_cb_t cb, void *data, int x1, int y1, int x2, int y2);
void bresenham_skip_first(bresenham_cb_t cb, void *data,
			  int x1, int y1, int x2, int y2);
void bresenham_skip_last(bresenham_cb_t cb, void *data,
			 int x1, int y1, int x2, int y2);
void bresenham_skip_first_last(bresenham_cb_t cb, void *data,
			       int x1, int y1, int x2, int y2);

#endif
