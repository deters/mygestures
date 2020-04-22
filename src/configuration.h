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

#ifndef MYGESTURES_Context_H_
#define MYGESTURES_Context_H_

#include <regex.h>

/* the movements */
enum STROKES
{
	NONE,
	LEFT,
	RIGHT,
	UP,
	DOWN
};

typedef struct movement_
{
	char *name;
	void *expression;
	regex_t *expression_compiled;
} Movement;

typedef struct context_
{
	char *name;
	char *title;
	char *class;

	struct context_ *parent;

	struct gesture_ **gesture_list;
	int gesture_count;

	Movement **movement_list;
	int movement_count;

	struct context_ **context_list;
	int context_count;

	regex_t *title_compiled;
	regex_t *class_compiled;

} Context;

typedef struct gesture_
{
	Context *context;
	Movement *movement;
	char *action;
} Gesture;

typedef struct active_window_info_
{
	char *title;
	char *class;
} ActiveWindowInfo;

typedef struct capture_
{
	int expression_count;
	char **expression_list;
	ActiveWindowInfo *active_window_info;
} Capture;

Context *configuration_create_context(Context *self,
									  char *context_name,
									  char *window_title,
									  char *window_class);
Gesture *configuration_create_gesture(Context *self, char *gesture_name, char *action);
Movement *configuration_create_movement(Context *self,
										char *movement_name,
										char *movement_expression);
Movement *configuration_find_movement_by_name(Context *self, char *movement_name);
Gesture *configuration_process_gesture(Context *self, Capture *capture);

#endif
