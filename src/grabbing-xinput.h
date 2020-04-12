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

/* names of the modifier keys */
extern const char *modifiers_names[];

/* valid strokes */
extern const char _STROKE_CHARS[];

Grabber *grabber_new(char *device_name, int button);

void grabber_finalize(Grabber *self);
void grabber_print_devices(Grabber *self);
void grabber_any_modifier(Grabber *self, int enable);
void grabber_list_devices(Grabber *self);
void grabber_follow_pointer(Grabber *self, int enable);
void grabber_focus(Grabber *self, int enable);

#endif /* MYGESTURES_GRABBING_H_ */
