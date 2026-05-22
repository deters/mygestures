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

#include "actions.h"
#include "configuration.h"
#include "uinput_device.h"

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
			exit(-1);
		}
		key->next = alloc_key_press();
		key = key->next;
		key->key = k;
		token = strsep(&copy, "+\n ");
	}

	base.next->original_str = str_ptr;
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

	release_keys_reverse(dpy, first_key);

	return;
}

void execute_wayland_action(Action *action) {
	const char *swaysock = getenv("SWAYSOCK");
	const char *hyprland_sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");

	if (swaysock) {
		switch (action->type) {
			case ACTION_ICONIFY:
				system("swaymsg move scratchpad");
				break;
			case ACTION_KILL:
				system("swaymsg kill");
				break;
			case ACTION_RAISE:
				system("swaymsg focus");
				break;
			case ACTION_LOWER:
				fprintf(stderr, "Warning: Lower action is not natively supported under Sway tiling layout.\n");
				break;
			case ACTION_MAXIMIZE:
				system("swaymsg fullscreen enable");
				break;
			case ACTION_RESTORE:
				system("swaymsg fullscreen disable");
				break;
			case ACTION_TOGGLE_MAXIMIZED:
				system("swaymsg fullscreen toggle");
				break;
			default:
				fprintf(stderr, "Warning: Wayland action %s is not implemented or supported under Sway.\n",
						get_action_name(action->type));
				break;
		}
		return;
	}

	if (hyprland_sig) {
		switch (action->type) {
			case ACTION_ICONIFY:
				system("hyprctl dispatch movetoworkspacesilent special:minimized");
				break;
			case ACTION_KILL:
				system("hyprctl dispatch killactive");
				break;
			case ACTION_RAISE:
				system("hyprctl dispatch alterzorder top");
				break;
			case ACTION_LOWER:
				system("hyprctl dispatch alterzorder bottom");
				break;
			case ACTION_MAXIMIZE:
				system("hyprctl dispatch fullscreen 1");
				break;
			case ACTION_RESTORE:
				system("hyprctl dispatch fullscreen 1"); // Toggles maximize back to normal
				break;
			case ACTION_TOGGLE_MAXIMIZED:
				system("hyprctl dispatch fullscreen 1");
				break;
			default:
				fprintf(stderr, "Warning: Wayland action %s is not implemented or supported under Hyprland.\n",
						get_action_name(action->type));
				break;
		}
		return;
	}

	// Fallback to simulating standard shortcuts via uinput virtual keyboard
	const char *desktop = getenv("XDG_CURRENT_DESKTOP");
	int is_gnome = 0;
	int is_kde = 0;
	if (desktop) {
		if (strstr(desktop, "GNOME") != NULL || strstr(desktop, "gnome") != NULL) {
			is_gnome = 1;
		}
		if (strstr(desktop, "KDE") != NULL || strstr(desktop, "kde") != NULL) {
			is_kde = 1;
		}
	}

	if (is_gnome) {
		switch (action->type) {
			case ACTION_ICONIFY:
				action_keypress(NULL, "Super_L+h");
				break;
			case ACTION_KILL:
				action_keypress(NULL, "Alt_L+F4");
				break;
			case ACTION_RAISE:
				// Window is focused upon click/grab start, so this is generally a no-op
				break;
			case ACTION_LOWER:
				action_keypress(NULL, "Alt_L+Escape");
				break;
			case ACTION_MAXIMIZE:
				action_keypress(NULL, "Super_L+Up");
				break;
			case ACTION_RESTORE:
				action_keypress(NULL, "Super_L+Down");
				break;
			case ACTION_TOGGLE_MAXIMIZED:
				action_keypress(NULL, "Alt_L+F10");
				break;
			default:
				fprintf(stderr, "Warning: Wayland action %s is not implemented or supported under GNOME.\n",
						get_action_name(action->type));
				break;
		}
	} else if (is_kde) {
		switch (action->type) {
			case ACTION_ICONIFY:
				action_keypress(NULL, "Alt_L+F9");
				break;
			case ACTION_KILL:
				action_keypress(NULL, "Alt_L+F4");
				break;
			case ACTION_RAISE:
				// Window is focused upon click/grab start, so this is generally a no-op
				break;
			case ACTION_LOWER:
				action_keypress(NULL, "Alt_L+Escape");
				break;
			case ACTION_MAXIMIZE:
				action_keypress(NULL, "Super_L+Page_Up");
				break;
			case ACTION_RESTORE:
				action_keypress(NULL, "Super_L+Page_Down");
				break;
			case ACTION_TOGGLE_MAXIMIZED:
				action_keypress(NULL, "Super_L+Page_Up");
				break;
			default:
				fprintf(stderr, "Warning: Wayland action %s is not implemented or supported under KDE.\n",
						get_action_name(action->type));
				break;
		}
	} else {
		// General fallback
		switch (action->type) {
			case ACTION_ICONIFY:
				action_keypress(NULL, "Super_L+h");
				break;
			case ACTION_KILL:
				action_keypress(NULL, "Alt_L+F4");
				break;
			case ACTION_RAISE:
				break;
			case ACTION_LOWER:
				action_keypress(NULL, "Alt_L+Escape");
				break;
			case ACTION_MAXIMIZE:
				action_keypress(NULL, "Super_L+Up");
				break;
			case ACTION_RESTORE:
				action_keypress(NULL, "Super_L+Down");
				break;
			case ACTION_TOGGLE_MAXIMIZED:
				action_keypress(NULL, "Alt_L+F10");
				break;
			default:
				fprintf(stderr, "Warning: Wayland action %s is not implemented or supported.\n",
						get_action_name(action->type));
				break;
		}
	}
}

