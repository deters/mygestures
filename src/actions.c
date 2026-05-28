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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "actions.h"
#include "configuration.h"
#include "uinput_device.h"
#include "wayland.h"
#include "logging.h"
#include <unistd.h>

/* Actions */
const char * action_name[ACTION_COUNT] = {
		"ERROR", "EXIT_GEST", "EXECUTE", "ICONIFY", "KILL", "RECONF", "RAISE", "LOWER", "MAXIMIZE",
		"RESTORE", "TOGGLE_MAXIMIZED", "KEYPRESS", "ABORT", "LAST" };

const char * get_action_name(int action) {
	return action_name[action];
}
;

enum {
	_NET_WM_STATE_REMOVE = 0, _NET_WM_STATE_ADD = 1, _NET_WM_STATE_TOGGLE = 2
};

/*
 * Iconify the focused window at given display.
 *
 * PUBLIC
 */
void action_iconify(Display *dpy, Window w) {
	if (w != None)
		XIconifyWindow(dpy, w, 0);

	return;
}

/**
 * Kill focused window at the given Display.
 *
 * PUBLIC
 */
void action_kill(Display *dpy, Window w) {

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
void action_raise(Display *dpy, Window w) {
	XRaiseWindow(dpy, w);
	return;
}

/**
 * Lower the focused window at the given Display.
 *
 * PUBLIC
 */
void action_lower(Display *dpy, Window w) {
	XLowerWindow(dpy, w);
	return;
}

/*
 * Taken from wmctrl
 */
static int client_msg(	Display *disp,
						Window win,
						char *msg,
						unsigned long data0,
						unsigned long data1,
						unsigned long data2,
						unsigned long data3,
						unsigned long data4) {
	XEvent event;
	long mask = SubstructureRedirectMask | SubstructureNotifyMask;

	event.xclient.type = ClientMessage;
	event.xclient.serial = 0;
	event.xclient.send_event = True;
	event.xclient.message_type = XInternAtom(disp, msg, False);
	event.xclient.window = win;
	event.xclient.format = 32;
	event.xclient.data.l[0] = data0;
	event.xclient.data.l[1] = data1;
	event.xclient.data.l[2] = data2;
	event.xclient.data.l[3] = data3;
	event.xclient.data.l[4] = data4;

	if (XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event)) {
		return EXIT_SUCCESS;
	} else {
		fprintf(stderr, "Cannot send %s event.\n", msg);
		return EXIT_FAILURE;
	}

	XFlush(disp);

}

/**
 * Maximize the focused window at the given Display.
 *
 * PUBLIC
 */
void action_toggle_maximized(Display *dpy, Window w) {

	unsigned long action;
	Atom prop1 = 0;
	Atom prop2 = 0;

	action = _NET_WM_STATE_TOGGLE;

	char *tmp_prop2, *tmp2;
	tmp_prop2 = "_NET_WM_STATE_MAXIMIZED_HORZ";
	prop2 = XInternAtom(dpy, tmp_prop2, False);
	char * tmp_prop1 = "_NET_WM_STATE_MAXIMIZED_VERT";

	prop1 = XInternAtom(dpy, tmp_prop1, False);

	client_msg(dpy, w, "_NET_WM_STATE", action, (unsigned long) prop1, (unsigned long) prop2, 0, 0);

}

/**
 * Maximize the focused window at the given Display.
 *
 * PUBLIC
 */
void action_restore(Display *dpy, Window w) {

	unsigned long action;
	Atom prop1 = 0;
	Atom prop2 = 0;

	action = _NET_WM_STATE_REMOVE;

	char *tmp_prop2, *tmp2;
	tmp_prop2 = "_NET_WM_STATE_MAXIMIZED_HORZ";
	prop2 = XInternAtom(dpy, tmp_prop2, False);
	char * tmp_prop1 = "_NET_WM_STATE_MAXIMIZED_VERT";

	prop1 = XInternAtom(dpy, tmp_prop1, False);

	client_msg(dpy, w, "_NET_WM_STATE", action, (unsigned long) prop1, (unsigned long) prop2, 0, 0);

}

/**
 * Maximize the focused window at the given Display.
 *
 * PUBLIC
 */
void action_maximize(Display *dpy, Window w) {

	unsigned long action;
	Atom prop1 = 0;
	Atom prop2 = 0;

	action = _NET_WM_STATE_ADD;

	char *tmp_prop2, *tmp2;
	tmp_prop2 = "_NET_WM_STATE_MAXIMIZED_HORZ";
	prop2 = XInternAtom(dpy, tmp_prop2, False);
	char * tmp_prop1 = "_NET_WM_STATE_MAXIMIZED_VERT";

	prop1 = XInternAtom(dpy, tmp_prop1, False);

	client_msg(dpy, w, "_NET_WM_STATE", action, (unsigned long) prop1, (unsigned long) prop2, 0, 0);

}

/**
 * Fake key event
 */
void press_key(Display *dpy, KeySym key, Bool is_press) {
	if (dpy == NULL) {
		uinput_keypress(NULL, key, is_press);
	} else {
		XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, key), is_press, CurrentTime);
	}
	return;
}

/* alloc a key_press struct ???? */
struct key_press * alloc_key_press(void) {
	struct key_press *ans = malloc(sizeof(struct key_press));
	bzero(ans, sizeof(struct key_press));
	return ans;
}

/**
 * Creates a Keysym from a char sequence
 *
 * PRIVATE
 */
struct key_press * string_to_keypress(char *str_ptr) {

	char * copy = strdup(str_ptr);

	struct key_press base;
	struct key_press *key;
	KeySym k;
	char *str = copy;
	char *token = str;
	char *str_dup;

	if (str == NULL)
		return NULL;

	key = &base;
	token = strsep(&copy, "+\n ");
	while (token != NULL) {
		/* printf("found : %s\n", token); */
		k = XStringToKeysym(token);
		if (k == NoSymbol) {
			fprintf(stderr, "error converting %s to keysym\n", token);
			// Do not exit, just skip or return what we have so far
			continue;
		}
		key->next = alloc_key_press();
		key = key->next;
		key->key = k;
		token = strsep(&copy, "+\n ");
	}

	base.next->original_str = str_ptr;
	free(str);
	return base.next;
}

static void release_keys_reverse(Display *dpy, struct key_press *key) {
	if (key == NULL)
		return;
	release_keys_reverse(dpy, key->next);
	press_key(dpy, key->key, False);
}

/**
 * Fake sequence key events
 */
void action_keypress(Display *dpy, char *data) {

	struct key_press * keys = string_to_keypress(data);

	struct key_press *first_key;
	struct key_press *tmp;

	first_key = (struct key_press *) keys;

	if (first_key == NULL) {
		fprintf(stderr, " internal error in %s, key is null\n", __func__);
		return;
	}

	for (tmp = first_key; tmp != NULL; tmp = tmp->next) {
		press_key(dpy, tmp->key, True);
	}

	usleep(20000); // 20ms delay to ensure combination is registered

	release_keys_reverse(dpy, first_key);

	while (first_key != NULL) {
		struct key_press *next = first_key->next;
		free(first_key);
		first_key = next;
	}

	return;
}

/**
 * Executes an action in a system-agnostic way.
 *
 * PUBLIC
 */
void execute_action(Display *dpy, Window focused_window, Action *action) {
	int id;

	assert(action);

	if (action->type == ACTION_EXECUTE) {
		id = fork();
		if (id == 0) {
			int i = system(action->original_str);
			exit(i);
		}
		if (id < 0) {
			LOG_ERROR("Error forking.\n");
		}
		return;
	}

	if (!dpy) {
		if (action->type == ACTION_KEYPRESS) {
			action_keypress(dpy, action->original_str);
		} else {
			execute_wayland_action(action);
		}
		return;
	}

	switch (action->type) {
	case ACTION_ICONIFY:
		action_iconify(dpy, focused_window);
		break;
	case ACTION_KILL:
		action_kill(dpy, focused_window);
		break;
	case ACTION_RAISE:
		action_raise(dpy, focused_window);
		break;
	case ACTION_LOWER:
		action_lower(dpy, focused_window);
		break;
	case ACTION_MAXIMIZE:
		action_maximize(dpy, focused_window);
		break;
	case ACTION_RESTORE:
		action_restore(dpy, focused_window);
		break;
	case ACTION_TOGGLE_MAXIMIZED:
		action_toggle_maximized(dpy, focused_window);
		break;
	case ACTION_KEYPRESS:
		action_keypress(dpy, action->original_str);
		break;
	default:
		LOG_ERROR("found an unknown gesture \n");
	}

	if (dpy) {
		XAllowEvents(dpy, 0, CurrentTime);
	}

	return;
}



