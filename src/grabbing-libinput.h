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

#ifndef LIBINPUT_GRABBING_H_
#define LIBINPUT_GRABBING_H_

#include "mygestures.h"

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

    Mygestures *mygestures;
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

    int delta_min;

    int synaptics;

    int shut_down;

} LibinputGrabber;

/* names of the modifier keys */
extern const char *modifiers_names[];

/* valid strokes */
extern const char _STROKE_CHARS[];

LibinputGrabber *grabber_Libinput_new(char *device_name, int button);
void grabber_Libinput_loop(LibinputGrabber *self, Mygestures *mygestures);
// void grabber_finalize(LibinputGrabber *self);
// void grabber_print_devices(LibinputGrabber *self);
// void grabber_any_modifier(LibinputGrabber *self, int enable);
void grabber_Libinput_list_devices(LibinputGrabber *self);
// void grabber_follow_pointer(LibinputGrabber *self, int enable);
// void grabber_focus(Grabber *LibinputGrabber, int enable);

#endif /* LIBINPUT_GRABBING_H_ */
