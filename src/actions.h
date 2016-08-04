/*
 Copyright 2008-2016 Lucas Augusto Deters
 Copyright 2005 Nir Tzachar

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 one line to give the program's name and an idea of what it does.
  */

#ifndef MYGESTURES_ACTIONS_H_
#define MYGESTURES_ACTIONS_H_

#include <X11/Xlib.h>

#define ACTION_COUNT 14

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
	ACTION_RESTORE,
	ACTION_TOGGLE_MAXIMIZED,
	ACTION_KEYPRESS,
	ACTION_ABORT,
	ACTION_LAST
};

struct key_press {
	KeySym key;
	struct key_press * next;
	char *original_str;
};

const char * get_action_name(int action);

void action_iconify(Display *dpy, Window w);
void action_kill(Display *dpy, Window w);
void action_raise(Display *dpy, Window w);
void action_lower(Display *dpy, Window w);
void action_maximize(Display *dpy, Window w);
void action_restore(Display *dpy, Window w);
void action_toggle_maximized(Display *dpy, Window w);
void action_keypress(Display *dpy, char *data);

#endif
