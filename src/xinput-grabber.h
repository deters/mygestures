/*
 Copyright 2013-2016 Lucas Augusto Deters
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

#ifndef XINPUT_GRABBING_H_
#define XINPUT_GRABBING_H_

#include "gestures.h"

/* modifier keys */
enum
{
	SHIFT = 0,
	CTRL,
	ALT,
	WIN,
	SCROLL,
	NUM,
	CAPS,
	MOD_END
};

typedef struct
{

	Gestures *mygestures;
	Display *dpy;

	char *devicename;
	int deviceid;
	int is_direct_touch;

	int button;
	int any_modifier;
	int follow_pointer;
	int focus;

	int verbose;

	int opcode;
	int event;
	int error;

	int old_x;
	int old_y;

	int delta_min;

	int synaptics;

	int shut_down;

} XInputGrabber;

/* names of the modifier keys */
extern const char *modifiers_names[];

/* valid strokes */
extern const char _STROKE_CHARS[];

XInputGrabber *grabber_xinput_new(char *device_name, int button);
void grabber_xinput_loop(XInputGrabber *self, Gestures *mygestures);
// void grabber_finalize(XInputGrabber *self);
// void grabber_print_devices(XInputGrabber *self);
// void grabber_any_modifier(XInputGrabber *self, int enable);
void grabber_xinput_list_devices(XInputGrabber *self);
// void grabber_follow_pointer(XInputGrabber *self, int enable);
// void grabber_focus(Grabber *XInputGrabber, int enable);

#endif /* MYGESTURES_GRABBING_H_ */
