/*
  Copyright 2005 Nir Tzachar
  Copyright 2013, 2014 Lucas Augusto Deters

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
  void (*kill)(XButtonEvent *ev);
  void (*iconify)(XButtonEvent *ev);
  void (*raise)(XButtonEvent *ev);
  void (*lower)(XButtonEvent *ev);
  void (*maximize)(XButtonEvent *ev);
};


extern struct wm_helper generic_wm_helper;
extern struct wm_helper *wm_helper;

void generic_iconify(XButtonEvent *ev);
void generic_kill(XButtonEvent *ev);
void generic_raise(XButtonEvent *ev);
void generic_lower(XButtonEvent *ev);
void generic_maximize(XButtonEvent *ev);

#endif
