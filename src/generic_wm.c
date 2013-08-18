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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <stdio.h>
#include "wm.h"
#include "helpers.h"
#include "gestures.h"

// mouse click

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

/**
 * Iconize a window from event.
 */
void generic_iconify(XButtonEvent *ev) {
	Window w = get_window(ev, 0);
	if (w != None)
		XIconifyWindow(ev->display, w, 0);

	return;
}

/**
 * Kill a window from event.
 */
void generic_kill(XButtonEvent *ev) {
	Window w = get_window(ev, 0);

	/* dont kill root window */
	if (w == RootWindow(ev->display, DefaultScreen(ev->display)))
		return;

	XSync(ev->display, 0);
	XKillClient(ev->display, w);
	XSync(ev->display, 0);
	return;
}

/**
 * Raise a window from event.
 */
void generic_raise(XButtonEvent *ev) {
	Window w = get_window(ev, 0);
	XRaiseWindow(ev->display, w);
	return;
}

/**
 * Lower a window from event.
 */
void generic_lower(XButtonEvent *ev) {
	Window w = get_window(ev, 0);
	XLowerWindow(ev->display, w);
	return;
}

/**
 * Maximize a window from event.
 */
void generic_maximize(XButtonEvent *ev) {
	Window w = get_window(ev, 0);
	int width = XDisplayWidth(ev->display, DefaultScreen(ev->display));
	int heigth = XDisplayHeight(ev->display, DefaultScreen(ev->display));

	XMoveResizeWindow(ev->display, w, 0, 0, width, heigth - 50);

	return;
}
struct wm_helper generic_wm_helper = { .iconify = generic_iconify, .kill =
		generic_kill, .raise = generic_raise, .lower = generic_lower,
		.maximize = generic_maximize, };

/*
 * Sends a fake mouse button event to a window.
 */

void mouseClick(Display *display, Window w, int button) // THE BUTTON STILL UNUSED
{
	XTestFakeButtonEvent(display, button, True, CurrentTime);
	XTestFakeButtonEvent(display, button, False, CurrentTime + 100);
}

/**
 * Creates a Keysym from a char sequence
 */
void *compile_key_action(char *str_ptr) {
	struct key_press base;
	struct key_press *key;
	KeySym k;
	char *str = str_ptr;
	char *token = str;
	char *str_dup;

	if (str == NULL)
		return NULL;

	/* do this before strsep.. */
	str_dup = strdup(str);

	key = &base;
	token = strsep(&str_ptr, "+\n ");
	while (token != NULL) {
		/* printf("found : %s\n", token); */
		k = XStringToKeysym(token);
		if (k == NoSymbol) {
			fprintf(stderr, "error converting %s to keysym\n", token);
			exit(-1);
		}
		key->next = alloc_key_press();
		key = key->next;
		key->key = k;
		token = strsep(&str_ptr, "+\n ");
	}

	base.next->original_str = str_dup;
	;
	return base.next;
}

Window I18N_get_parent_win(Display *dpy, Window w) {
	Window root_return, parent_return, *child_return;
	unsigned int nchildren_return;
	int ret;
	ret = XQueryTree(dpy, w, &root_return, &parent_return, &child_return,
			&nchildren_return);

	return parent_return;
}

Status I18N_FetchName(Display *dpy, Window w, char **winname) {
	int status;
	XTextProperty text_prop;
	char **list;
	int num;

	status = XGetWMName(dpy, w, &text_prop);
	if (!status || !text_prop.value || !text_prop.nitems) {
		printf("getwmname error ");
		*winname = NULL;
		return 0;
	}
	status = Xutf8TextPropertyToTextList(dpy, &text_prop, &list, &num);
	if (status < Success || !num || !*list) {
		*winname = NULL;
		return 0;
	}
	XFree(text_prop.value);
	*winname = (char *) strdup(*list);
	XFreeStringList(list);
	return 1;
}

/*
 * Returns an struct identifying name and class of a window from and XButtonEvent
 */
struct window_info *getWindowInfo(XButtonEvent *e) {

	Window win = 0;
	int ret, val;
	ret = XGetInputFocus(e->display, &win, &val);

	if (val == RevertToParent) {
		win = I18N_get_parent_win(e->display, win);
	}

	char *win_title;
	ret = I18N_FetchName(e->display, win, &win_title);

	struct window_info *ans = malloc(sizeof(struct window_info));
	bzero(ans, sizeof(struct window_info));

	char *win_class = NULL;

	XClassHint class_hints;

	int result = XGetClassHint(e->display, win, &class_hints);

	if (class_hints.res_class != NULL)
		win_class = class_hints.res_class;

	if (win_class == NULL) {
		win_class = "";
	}

	ans->class = win_class;
	ans->title = win_title;

	return ans;

}



// TODO: the right click doesn't work with some Java Applications.
// TODO: create a method that classifies some window (Toolbar | Utility | Dialog | Normal | Unknown)

