/*
 * grabbing.h
 *
 *  Created on: Aug 31, 2013
 *      Author: deters
 */

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
