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
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include "wm.h"
#include "helpers.h"
#include "gestures.h"

// mouse click

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xutil.h>

enum
{
_NET_WM_STATE_REMOVE =0,
_NET_WM_STATE_ADD = 1,
_NET_WM_STATE_TOGGLE =2
};


/*
 * Emulate a mouse click at the given display.
 *
 * PRIVATE
 */
void mouse_click(Display *display, int button) {
	XTestFakeButtonEvent(display, button, True, CurrentTime);
	XTestFakeButtonEvent(display, button, False, CurrentTime+1);
}



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
		*out_window_title = "";
	}
	status = Xutf8TextPropertyToTextList(dpy, &text_prop, &list, &num);

	if (status < Success || !num || !*list) {
		*out_window_title = "";
	} else {
		*out_window_title = (char *) strdup(*list);
	}
	XFree(text_prop.value);
	XFreeStringList(list);

	return 1;
}





/*
 * Return a window_info struct for the focused window at a given Display.
 *
 * PRIVATE
 */
struct window_info * generic_get_window_context(Display *dpy) {

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

	if (win_class){
		ans->class = win_class;
	} else {
		ans->class = "";
	}

	if (win_title){
		ans->title = win_title;
	} else {
		ans->title = "";
	}



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
/*
	int width = XDisplayWidth(dpy, DefaultScreen(dpy));
	int heigth = XDisplayHeight(dpy, DefaultScreen(dpy));

	XMoveResizeWindow(dpy, w, 0, 0, width, heigth - 50);

	return;

*/
	XEvent xev;
	Atom wm_state  =  XInternAtom(dpy, "_NET_WM_STATE", False);
	Atom max_horz  =  XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	Atom max_vert  =  XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = w;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
	xev.xclient.data.l[1] = max_horz;
	xev.xclient.data.l[2] = max_vert;

	XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureNotifyMask, &xev);

	fprintf(stderr,"maximizou\n");

	return;

}


/**
 * Fake key event
 */
void press_key(Display *dpy, KeySym key, Bool is_press) {

	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, key), is_press, CurrentTime);
	return;
}

/**
 * Fake sequence key events
 */
void generic_root_send(Display *dpy, struct key_press *data) {
	struct key_press *first_key;
	struct key_press *tmp;

	first_key = (struct key_press *) data;

	if (first_key == NULL) {
		fprintf(stderr, " internal error in %s, key is null\n", __func__);
		return;
	}



	for (tmp = first_key; tmp != NULL; tmp = tmp->next){
		press_key(dpy, tmp->key, True);
	}


	for (tmp = first_key; tmp != NULL; tmp = tmp->next){
		press_key(dpy, tmp->key, False);
	}


	return;
}




/**
 * Execute an action
 */
void execute_action(Display *dpy, struct action *action) {
	int id;

	// if there is an action
	if (action != NULL) {

		switch (action->type) {
		case ACTION_EXECUTE:
			id = fork();
			if (id == 0) {
				int i = system(action->original_str);
				exit(i);
			}
			if (id < 0){
				fprintf(stderr, "Error forking.\n");
			}

			break;
		case ACTION_ICONIFY:
			action_helper->iconify(dpy, get_focused_window(dpy));
			break;
		case ACTION_KILL:
			action_helper->kill(dpy, get_focused_window(dpy));
			break;
		case ACTION_RAISE:
			action_helper->raise(dpy, get_focused_window(dpy));
			break;
		case ACTION_LOWER:
			action_helper->lower(dpy, get_focused_window(dpy));
			break;
		case ACTION_MAXIMIZE:
			action_helper->maximize(dpy, get_focused_window(dpy));
			break;
		case ACTION_ROOT_SEND:
			action_helper->root_send(dpy, action->data);
			break;
		default:
			fprintf(stderr, "found an unknown gesture \n");
		}
	}
	return;
}

struct action_helper generic_action_helper = { .iconify = generic_iconify, .kill =
		generic_kill, .raise = generic_raise, .lower = generic_lower,
		.maximize = generic_maximize, .root_send = generic_root_send, };




