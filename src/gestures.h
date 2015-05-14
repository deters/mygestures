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
#define LEFT_DIRECTION  'L'
#define RIGHT_DIRECTION  'R'
#define UP_DIRECTION  'U'
#define DOWN_DIRECTION  'D'
#define UPPER_RIGHT_DIRECTION  '9'
#define UPPER_LEFT_DIRECTION  '7'
#define BOTTOM_RIGHT_DIRECTION  '3'
#define BOTTOM_LEFT_DIRECTION  '1'

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
	struct context ** context_list;
	int context_count;
	struct movement ** movements;
	int movement_count;
	struct context * parent; /* weak */
	int abort;
	regex_t * title_compiled;
	regex_t * class_compiled;

};

struct gesture {
	char * name;
	//struct context *context;
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

struct context * gestures_init();

void gestures_finalize();

struct gesture * gesture_match(struct context * context,
		char * captured_sequence, char * window_class, char * window_title);

void gestures_set_config_file(char * config_file);

#endif
