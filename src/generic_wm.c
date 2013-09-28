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





struct action_helper generic_action_helper = { .iconify = generic_iconify, .kill =
		generic_kill, .raise = generic_raise, .lower = generic_lower,
		.maximize = generic_maximize, .root_send = generic_root_send, };




