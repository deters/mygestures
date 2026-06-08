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

#include "configuration.h"

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

/* names of the modifier keys */
extern const char *modifiers_names[];

/* valid strokes */
extern const char _STROKE_CHARS[];

typedef struct
{
	char *devicename;
	int deviceid;
	int is_direct_touch;

	int button;
	int any_modifier;
	int follow_pointer;
	int focus;

	int started;
	int verbose;

	int old_x;
	int old_y;

	int delta_min;

	int evdev;
	int is_exclusive;

	Point2D *captured_points;
	int captured_count;
	int captured_capacity;

	int shut_down;

} Grabber;

Grabber *grabber_new(char *device_name, int button);
void grabber_loop(Grabber *self, Configuration *conf);
void grabbing_start_movement(Grabber *self, int new_x, int new_y);
void grabbing_update_movement(Grabber *self, int new_x, int new_y);
void grabbing_end_movement(Grabber *self, int new_x, int new_y,
						   char *device_name, Configuration *conf);
Point2D *grabbing_simplify_points(const Point2D *points, int count, double epsilon, int *out_count);

void grabber_finalize(Grabber *self);
void grabber_print_devices(Grabber *self);
void grabber_any_modifier(Grabber *self, int enable);
void grabber_list_devices(Grabber *self);
void grabber_follow_pointer(Grabber *self, int enable);
void grabber_focus(Grabber *self, int enable);

#endif /* MYGESTURES_GRABBING_H_ */
