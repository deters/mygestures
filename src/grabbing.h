/*
  Copyright 2013, 2014 Lucas Augusto Deters

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.  */

#ifndef GRABBING_H_
#define GRABBING_H_
#endif /* GRABBING_H_ */

#include <X11/Xlib.h>

int grabbing_init();

void grabbing_set_button(int b);
void grabbing_set_without_brush(int b);
void grabbing_set_button_modifier(char * button_modifier_str);
void grabbing_set_brush_color(char * color);
void grabbing_event_loop();
void grabbing_finalize();
