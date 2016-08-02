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

typedef struct movement_ {
	char *name;
	void *expression;
	regex_t * expression_compiled;
} Movement;

typedef struct context_ {
	char *name;
	char *title;
	char *class;

	struct user_configuration_ * parent_user_configuration;

	struct gesture_ ** gesture_list;
	int gesture_count;

	int abort;
	regex_t * title_compiled;
	regex_t * class_compiled;

} Context;

typedef struct user_configuration_ {

	Movement** movement_list;
	int movement_count;

	Context ** context_list;
	int context_count;
} Configuration;

typedef struct action_ {
	int type;
	//struct key_press *data;
	char *original_str;
} Action;

typedef struct gesture_ {
	char * name;
	Context *context;
	Movement *movement;
	Action ** action_list;
	int action_count;
} Gesture;

typedef struct window_info_ {
	char *title;
	char *class;
} Window_info;

typedef struct capture_ {
	int expression_count;
	char ** expression_list;
	Window_info * active_window_info;
} Capture;

Configuration * configuration_new();

Context * configuration_create_context(Configuration * self, char * context_name, char *window_title, char *window_class);
Gesture * configuration_create_gesture(Context * self, char * gesture_name, char * gesture_movement);
Movement * configuration_create_movement(Configuration * self, char *movement_name, char *movement_expression);
Action * configuration_create_action(Gesture * self, int action_type, char * original_str);
Movement * configuration_find_movement_by_name(Configuration * self, char * movement_name);
int configuration_get_gestures_count(Configuration * self);
Gesture * configuration_process_gesture(Configuration * self, Capture * capture);

#endif
