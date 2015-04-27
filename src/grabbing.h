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
#include "gestures.h"

struct captured_movements {
	char *basic_movements;
	char *advanced_movements;
	char *window_title;
	char *window_class;
};


struct key_press {
	KeySym key;
	struct key_press * next;
};


int grabbing_init();
void grabbing_run();

void grabbing_set_button(int b);
void grabbing_set_without_brush(int b);
void grabbing_set_button_modifier(char * button_modifier_str);
void grabbing_set_brush_color(char * color);
struct captured_movements * grabbing_capture_movements();
void grabbing_finalize();

struct key_press * string_to_keypress(char *str_ptr);
void mouse_click(Display *display, int button);

void grabbing_iconify();
void grabbing_kill();
void grabbing_raise();
void grabbing_lower();
void grabbing_maximize();
void grabbing_root_send(char * keys);

