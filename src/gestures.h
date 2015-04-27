/*
 Copyright 2005 Nir Tzachar
 Copyright 2013 Lucas Augusto Deters

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


/* the movements */
enum STROKES {
	NONE, LEFT, RIGHT, UP, DOWN, ONE, THREE, SEVEN, NINE
};




struct movement {
	char *name;
	void *expression;
	regex_t * compiled;
};



struct context {
	char *name;
	char *title;
	char *class;
	struct gesture ** gestures;
	int gestures_count;
	int abort;
	regex_t * title_compiled;
	regex_t * class_compiled;

};

struct gesture {
	char * name;
	struct context *context;
	struct movement *movement;
	struct action **actions;
	int actions_count;
};


struct gesture_engine {
	char * config_file;
};

/* Actions */
enum {
	ACTION_ERROR,
	ACTION_EXIT_GEST,
	ACTION_EXECUTE,
	ACTION_ICONIFY,
	ACTION_KILL,
	ACTION_RECONF,
	ACTION_RAISE,
	ACTION_LOWER,
	ACTION_MAXIMIZE,
	ACTION_ROOT_SEND,
	ACTION_ABORT,
	ACTION_LAST
};

struct action {
	int type;
	char *data;
};



int gestures_init();

struct gesture * gesture_match(char * captured_sequence, char * window_class,
		char * window_title);

void gestures_set_config_file(char * config_file);



#endif
