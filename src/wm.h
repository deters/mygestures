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


/* Actions */
enum {
	ACTION_ERROR,
	ACTION_EXIT_GEST,
	ACTION_EXECUTE,
	ACTION_ICONIFY,
	ACTION_KILL,
	ACTION_RECONF,
	ACTION_RAISE,
	ACTION_LOWER,
	ACTION_MAXIMIZE,
	ACTION_ROOT_SEND,
	ACTION_ABORT,
	ACTION_LAST
};

struct action {
	int type;
	struct key_press *data;
	char *original_str;
};

struct key_press {
	KeySym key;
	struct key_press * next;
	char *original_str;
};




struct action_helper {
  void (*kill)(Display *dpy, Window w);
  void (*iconify)(Display *dpy, Window w);
  void (*raise)(Display *dpy, Window w);
  void (*lower)(Display *dpy, Window w);
  void (*maximize)(Display *dpy, Window w);
  void (*root_send)(Display *dpy, struct key_press *data);
};


extern struct action_helper generic_action_helper;
extern struct action_helper *action_helper;

void generic_iconify(Display *dpy, Window w);
void generic_kill(Display *dpy, Window w);
void generic_raise(Display *dpy, Window w);
void generic_lower(Display *dpy, Window w);
void generic_maximize(Display *dpy, Window w);
void generic_root_send(Display *dpy, struct key_press *data);
struct window_info * generic_get_window_context(Display *dpy);


void execute_action(Display *dpy, struct action *action);
void mouse_click(Display *display, int button);


#endif
