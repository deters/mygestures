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

#ifndef MYGESTURES_CONFIGURATION_H_
#define MYGESTURES_CONFIGURATION_H_

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

typedef struct user_configuration_ {

	Movement** movement_list;
	int movement_count;

	struct gesture_ ** gesture_list;
	int gesture_count;
} Configuration;

typedef struct action_ {
	int type;
	char *original_str;
} Action;

typedef struct gesture_ {
	char * name;
	Movement *movement;
	Action ** action_list;
	int action_count;
} Gesture;

typedef struct capture_ {
	int expression_count;
	char ** expression_list;
} Capture;

Configuration * configuration_new();

Gesture * configuration_create_gesture(Configuration * self, char * gesture_name, char * gesture_movement_or_stroke);

Movement * configuration_create_movement(	Configuration * self,
											char *movement_name,
											char *movement_expression);

Action * configuration_create_action(Gesture * self, int action_type, char * original_str);

void configuration_add_action_from_string(Gesture * self, const char * action_str);

Movement * configuration_find_movement_by_name(Configuration * self, char * movement_name);
int configuration_get_gestures_count(Configuration * self);
Gesture * configuration_process_gesture(Configuration * self, Capture * capture);

void configuration_load_from_defaults(Configuration * configuration, int create_config);
void configuration_load_from_file(Configuration * configuration, char * filename);

#endif
