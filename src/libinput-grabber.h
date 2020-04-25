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

#include "gestures.h"

typedef struct
{

    Mygestures *mygestures;
    Display *dpy;

    char *devicename;
    int deviceid;
    int is_direct_touch;

    int event_count;

    int nfingers;
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

LibinputGrabber *libinput_grabber_new(char *device_name, int nfingers);
void libinput_grabber_loop(LibinputGrabber *self, Mygestures *mygestures);
void libinput_grabber_list_devices();

#endif /* LIBINPUT_GRABBING_H_ */
