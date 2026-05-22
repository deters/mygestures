/*
 Copyright 2026 Lucas Augusto Deters
 Copyright 2005 Nir Tzachar

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 */

#ifndef MYGESTURES_X11_WINDOW_H_
#define MYGESTURES_X11_WINDOW_H_

#include <X11/Xlib.h>
#include "configuration.h"

Status fetch_window_title(Display *dpy, Window w, char **out_window_title);
ActiveWindowInfo *get_active_window_info(Display *dpy, Window win);
Window get_parent_window(Display *dpy, Window w);
Window get_focused_window(Display *dpy);

#endif /* MYGESTURES_X11_WINDOW_H_ */
