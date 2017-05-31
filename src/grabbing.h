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


#ifndef MYGESTURES_GRABBING_H_
#define MYGESTURES_GRABBING_H_

#include <X11/Xlib.h>
#include "drawing/drawing-backing.h"
#include "drawing/drawing-brush.h"
#include "configuration.h"

/* modifier keys */
enum {
	SHIFT = 0, CTRL, ALT, WIN, SCROLL, NUM, CAPS, MOD_END
};

/* names of the modifier keys */
extern const char *modifiers_names[];

/* valid strokes */
extern const char stroke_representations[];

typedef struct {

	Display * dpy;

	char * devicename;
	int deviceid;
	int is_direct_touch;

	int button;

	int started;
	int verbose;

	int opcode;
	int event;
	int error;

	int old_x;
	int old_y;

	int delta_min;

	int synaptics;

	int rought_old_x;
	int rought_old_y;

	char * fine_direction_sequence;
	char * rought_direction_sequence;

	backing_t backing;
	brush_t brush;

	int shut_down;

	struct brush_image_t *brush_image;

} Grabber;

Grabber * grabber_new(char * device_name, int button);
void grabber_loop(Grabber * self, Configuration * conf);
void grabber_finalize(Grabber * self);
void grabber_print_devices(Grabber * self);

#endif /* MYGESTURES_GRABBING_H_ */

