// /*
//  Copyright 2008-2016 Lucas Augusto Deters
//  Copyright 2005 Nir Tzachar

//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2, or (at your option)
//  any later version.

//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.

//  one line to give the program's name and an idea of what it does.
//  */

// #if HAVE_CONFIG_H
// #include <config.h>
// #endif

// #include <X11/Xlib.h>
// #include <X11/extensions/XTest.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

// #include "actions.h"
// #include "configuration.h"

// /*
//  * Taken from wmctrl
//  */
// static int client_msg(Display *disp,
// 					  Window win,
// 					  char *msg,
// 					  unsigned long data0,
// 					  unsigned long data1,
// 					  unsigned long data2,
// 					  unsigned long data3,
// 					  unsigned long data4)
// {
// 	XEvent event;
// 	long mask = SubstructureRedirectMask | SubstructureNotifyMask;

// 	event.xclient.type = ClientMessage;
// 	event.xclient.serial = 0;
// 	event.xclient.send_event = True;
// 	event.xclient.message_type = XInternAtom(disp, msg, False);
// 	event.xclient.window = win;
// 	event.xclient.format = 32;
// 	event.xclient.data.l[0] = data0;
// 	event.xclient.data.l[1] = data1;
// 	event.xclient.data.l[2] = data2;
// 	event.xclient.data.l[3] = data3;
// 	event.xclient.data.l[4] = data4;

// 	if (XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event))
// 	{
// 		XFlush(disp);
// 		return EXIT_SUCCESS;
// 	}
// 	else
// 	{
// 		fprintf(stderr, "Cannot send %s event.\n", msg);
// 		return EXIT_FAILURE;
// 	}
// }

// /**
//  * Fake key event
//  */
// void press_key(Display *dpy, KeySym key, Bool is_press)
// {

// 	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, key), is_press, CurrentTime);
// 	return;
// }

// /* alloc a key_press struct ???? */
// struct key_press *alloc_key_press(void)
// {
// 	struct key_press *ans = malloc(sizeof(struct key_press));
// 	bzero(ans, sizeof(struct key_press));
// 	return ans;
// }

// /**
//  * Fake sequence key events
//  */
// void action_keypress(Display *dpy, char *data)
// {

// 	struct key_press *keys = create_key_chain(data);

// 	struct key_press *k = keys;

// 	while (k != NULL)
// 	{
// 		press_key(dpy, k->key, True);
// 		k = k->next;
// 	}

// 	k = keys;

// 	while (k != NULL)
// 	{
// 		press_key(dpy, k->key, False);
// 		k = k->next;
// 	}

// 	free_key_chain(keys);
// }
