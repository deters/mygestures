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

/* the movements */
enum STROKES {
	NONE, LEFT, RIGHT, UP, DOWN, ONE, THREE, SEVEN, NINE
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

typedef struct movement_ {
	char *name;
	void *expression;
	regex_t * compiled;
} Movement;

typedef struct context_ {
	char *name;
	char *title;
	char *class;

	struct engine_ * engine;

	struct gesture_ ** gestures;
	int gestures_count;

	int abort;
	regex_t * title_compiled;
	regex_t * class_compiled;

} Context;

typedef struct engine_ {

	Movement** movement_list;
	int movement_count;

	Context ** context_list;
	int context_count;
} Engine;

typedef struct action_ {
	int type;
	//struct key_press *data;
	char *original_str;
} Action;

typedef struct gesture_ {
	char * name;
	Context *context;
	Movement *movement;
	Action **actions;
	int actions_count;
} Gesture;

typedef struct window_info_ {
	char *title;
	char *class;
} Window_info;

typedef struct grabbed_ {
	int sequences_count;
	char ** sequences;
	Window_info * focused_window;
} Grabbed;

Engine * engine_new();


Context * engine_create_context(Engine * self, char * context_name, char *window_title, char *window_class);
Gesture * context_create_gesture(Context * self, char * gesture_name, char * gesture_movement);
Movement * engine_create_movement(Engine * self, char *movement_name, char *movement_expression);
Action * gesture_create_action(Gesture * self, int action_type, char * original_str);
Movement * engine_find_movement_by_name(Engine * self, char * movement_name);
Gesture * engine_process_gesture(Engine * self, Grabbed * grab);

#endif
