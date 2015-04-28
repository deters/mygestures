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

#define MOVEMENT_MAX_DIRECTIONS 64
#define NAMES_MAX_SIZE 64
#define GEST_EXTRA_DATA_MAX 4096

#define NO_DIRECTION  '\0'

#ifndef LEFT_DIRECTION
#define LEFT_DIRECTION  'L'
#endif

#ifndef RIGHT_DIRECTION
#define RIGHT_DIRECTION  'R'
#endif

#ifndef UP_DIRECTION
#define UP_DIRECTION  'U'
#endif

#ifndef DOWN_DIRECTION
#define DOWN_DIRECTION  'D'
#endif

#ifndef UPPER_RIGHT_DIRECTION
#define UPPER_RIGHT_DIRECTION  '9'
#endif

#ifndef UPPER_LEFT_DIRECTION
#define UPPER_LEFT_DIRECTION  '7'
#endif

#ifndef BOTTOM_RIGHT_DIRECTION
#define BOTTOM_RIGHT_DIRECTION  '3'
#endif

#ifndef BOTTOM_LEFT_DIRECTION
#define BOTTOM_LEFT_DIRECTION  '1'
#endif


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
