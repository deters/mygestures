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



/*
 * Get the parent window.
 *
 * PRIVATE
 */
Window get_parent_window(Display *dpy, Window w) {
	Window root_return, parent_return, *child_return;
	unsigned int nchildren_return;
	int ret;
	ret = XQueryTree(dpy, w, &root_return, &parent_return, &child_return,
			&nchildren_return);

	return parent_return;
}

/*
 * Get the title of a given window at out_window_title.
 *
 * PRIVATE
 */
Status fetch_window_title(Display *dpy, Window w, char **out_window_title) {
	int status;
	XTextProperty text_prop;
	char **list;
	int num;

	status = XGetWMName(dpy, w, &text_prop);
	if (!status || !text_prop.value || !text_prop.nitems) {
		printf("getwmname error ");
		*out_window_title = NULL;
		return 0;
	}
	status = Xutf8TextPropertyToTextList(dpy, &text_prop, &list, &num);
	if (status < Success || !num || !*list) {
		*out_window_title = NULL;
		return 0;
	}
	XFree(text_prop.value);
	*out_window_title = (char *) strdup(*list);
	XFreeStringList(list);
	return 1;
}


/*
 * Return a window_info struct for the focused window at a given Display.
 *
 * PRIVATE
 */
struct window_info * get_window_context(Display *dpy) {

	Window win = 0;
	int ret, val;
	ret = XGetInputFocus(dpy, &win, &val);

	if (val == RevertToParent) {
		win = get_parent_window(dpy, win);
	}

	char *win_title;
	ret = fetch_window_title(dpy, win, &win_title);

	struct window_info *ans = malloc(sizeof(struct window_info));
	bzero(ans, sizeof(struct window_info));

	char *win_class = NULL;

	XClassHint class_hints;

	int result = XGetClassHint(dpy, win, &class_hints);

	if (class_hints.res_class != NULL)
		win_class = class_hints.res_class;

	if (win_class == NULL) {
		win_class = "";
	}

	ans->class = win_class;
	ans->title = win_title;

	return ans;

}

/*
 * Return the focused window at the given display.
 *
 * PRIVATE
 */
Window get_focused_window(Display *dpy) {

	Window win = 0;
	int ret, val;
	ret = XGetInputFocus(dpy, &win, &val);

	if (val == RevertToParent) {
		win = get_parent_window(dpy, win);
	}

	return win;

}

/*
 * Iconify the focused window at given display.
 *
 * PUBLIC
 */
void generic_iconify(Display *dpy, Window w) {
	if (w != None)
		XIconifyWindow(dpy, w, 0);

	return;
}


/**
 * Kill focused window at the given Display.
 *
 * PUBLIC
 */
void generic_kill(Display *dpy, Window w) {

	/* dont kill root window */
	if (w == RootWindow(dpy, DefaultScreen(dpy)))
		return;

	XSync(dpy, 0);
	XKillClient(dpy, w);
	XSync(dpy, 0);
	return;
}

/**
 * Raise the focused window at the given Display.
 *
 * PUBLIC
 */
void generic_raise(Display *dpy, Window w) {
	XRaiseWindow(dpy, w);
	return;
}

/**
 * Lower the focused window at the given Display.
 *
 * PUBLIC
 */
void generic_lower(Display *dpy, Window w) {
	XLowerWindow(dpy, w);
	return;
}

/**
 * Maximize the focused window at the given Display.
 *
 * PUBLIC
 */
void generic_maximize(Display *dpy, Window w) {

	int width = XDisplayWidth(dpy, DefaultScreen(dpy));
	int heigth = XDisplayHeight(dpy, DefaultScreen(dpy));

	XMoveResizeWindow(dpy, w, 0, 0, width, heigth - 50);

	return;
}

struct wm_helper generic_wm_helper = { .iconify = generic_iconify, .kill =
		generic_kill, .raise = generic_raise, .lower = generic_lower,
		.maximize = generic_maximize, };




