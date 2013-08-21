/*
 Copyright 2005 Nir Tzachar

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.  */

#ifndef __GESTURES_h
#define __GESTURES_h
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <regex.h>

#define GEST_SEQUENCE_MAX 64
#define GEST_ACTION_NAME_MAX 32
#define GEST_EXTRA_DATA_MAX 4096

#define CONTROL_L_MASK (1<<0)
#define CONTROL_R_MASK (1<<1)
#define SHIFT_L_MASK (1<<2)
#define SHIFT_R_MASK (1<<3)
#define ALT_L_MASK (1<<4)
#define ALT_R_MASK (1<<5)
#define TAB_MASK (1<<6)

struct movement {
	char *name;
	void *expression;
	regex_t compiled;
};



struct context {
	char *title;
	char *class;
	regex_t title_compiled;
	regex_t class_compiled;
};

struct gesture {
	struct context *context;
	struct movement *movement;
	struct action *action;
	regex_t movement_compiled;
};



struct window_info {
	char *title;
	char *class;
};

int init_gestures(char *config_file);

void * compile_key_action(char *str_ptr);

struct gesture * process_movement_sequences(Display * dpy,
		struct window_info *current_context, char *complex_sequence,
		char * simple_sequence);

#endif
