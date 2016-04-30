/*
 Copyright 2005 Nir Tzachar

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.  */

#ifndef __WM_H__
#define __WM_H__
#include <X11/Xlib.h>

#include "gestures.h"

struct key_press {
	KeySym key;
	struct key_press * next;
	char *original_str;
};

void generic_iconify(Display *dpy, Window w);
void generic_kill(Display *dpy, Window w);
void generic_raise(Display *dpy, Window w);
void generic_lower(Display *dpy, Window w);
void generic_maximize(Display *dpy, Window w);
void generic_root_send(Display *dpy, char *data);

#endif
