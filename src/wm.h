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

struct wm_helper {
  void (*kill)(Display *dpy);
  void (*iconify)(Display *dpy);
  void (*raise)(Display *dpy);
  void (*lower)(Display *dpy);
  void (*maximize)(Display *dpy);
};


extern struct wm_helper generic_wm_helper;
extern struct wm_helper *wm_helper;

void generic_iconify(Display *dpy);
void generic_kill(Display *dpy);
void generic_raise(Display *dpy);
void generic_lower(Display *dpy);
void generic_maximize(Display *dpy);

#endif
