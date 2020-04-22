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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <assert.h>

#include "configuration.h"

void context_set_title(Context *context, char *window_title)
{

	assert(context);
	assert(window_title);

	context->title = strdup(window_title);

	regex_t *title_compiled = NULL;
	if (context->title)
	{
		title_compiled = malloc(sizeof(regex_t));
		if (regcomp(title_compiled, window_title, REG_EXTENDED | REG_NOSUB))
		{
			fprintf(stderr, "Error compiling regexp: %s\n", window_title);
			free(title_compiled);
			title_compiled = NULL;
		}
	}
	context->title_compiled = title_compiled;
}

void context_set_class(Context *context, char *window_class)
{

	assert(context);
	assert(window_class);

	context->class = strdup(window_class);
	regex_t *class_compiled = NULL;
	if (context->class)
	{
		class_compiled = malloc(sizeof(regex_t));
		if (regcomp(class_compiled, window_class, REG_EXTENDED | REG_NOSUB))
		{
			fprintf(stderr, "Error compiling regexp: %s\n", window_class);
			free(class_compiled);
			class_compiled = NULL;
		}
	}
	context->class_compiled = class_compiled;
}

/* alloc a window struct */
Context *configuration_create_context(Context *parent, char *context_name,
									  char *window_title, char *window_class)
{

	assert(context_name);
	assert(window_title);
	assert(window_class);

	Context *self = malloc(sizeof(Context));

	bzero(self, sizeof(Context));

	if (context_name)
	{
		self->name = strdup(context_name);
	}
	else
	{
		self->name = "All applications";
	}

	self->parent = parent;

	context_set_title(self, window_title);
	context_set_class(self, window_class);

	self->movement_list = malloc(sizeof(Movement *) * 255);
	self->movement_count = 0;

	self->gesture_list = malloc(sizeof(Gesture *) * 255);
	self->gesture_count = 0;

	if (parent)
	{
		parent->context_list[parent->context_count++] = self;
	}

	return self;
}

void movement_set_expression(Movement *movement, char *movement_expression)
{
	movement->expression = movement_expression;
	char *regex_str = malloc(sizeof(char) * (strlen(movement_expression) + 5));
	strcpy(regex_str, "");
	strcat(regex_str, "^(");
	strcat(regex_str, movement_expression);
	strcat(regex_str, ")$");
	regex_t *movement_compiled = NULL;
	movement_compiled = malloc(sizeof(regex_t));
	if (regcomp(movement_compiled, regex_str, REG_EXTENDED | REG_NOSUB) != 0)
	{
		fprintf(stderr, "Warning: Invalid movement sequence: %s\n", regex_str);
		free(movement_compiled);
		movement_compiled = NULL;
	}
	else
	{
		regcomp(movement_compiled, regex_str, REG_EXTENDED | REG_NOSUB);
	}
	free(regex_str);
	movement->expression_compiled = movement_compiled;
}

/* alloc a movement struct */
Movement *configuration_create_movement(Context *self,
										char *movement_name, char *movement_expression)
{

	assert(self);
	assert(movement_expression);

	Movement *movement = malloc(sizeof(Movement));
	bzero(movement, sizeof(Movement));

	movement->name = strdup(movement_name);
	movement_set_expression(movement, strdup(movement_expression));

	self->movement_list[self->movement_count] = movement;
	self->movement_count++;

	return movement;
}

Gesture *configuration_create_gesture(Context *self,
									  char *gesture_movement,
									  char *action)
{

	assert(self);
	assert(gesture_movement);

	Gesture *ans = malloc(sizeof(Gesture));
	bzero(ans, sizeof(Gesture));

	ans->movement = configuration_find_movement_by_name(
		self, gesture_movement);

	if (!ans->movement)
	{
		printf(
			"Movement '%s' is unknown. The gesture will be inaccessible.\n",
			gesture_movement);
	}

	ans->context = self;
	ans->action = strdup(action);

	self->gesture_list[self->gesture_count++] = ans;

	return ans;
}

Gesture *match_gesture(Context *self, char *captured_sequence,
					   ActiveWindowInfo *window)
{

	assert(self);
	assert(captured_sequence);
	assert(window);

	int c = 0;

	Gesture *match = NULL;

	for (c = 0; c < self->context_count; ++c)
	{

		Context *child_context = self->context_list[c];

		match = match_gesture(child_context, captured_sequence, window);
		if (match)
		{
			return match;
		}
	}

	assert(self->class);
	assert(self->title);

	if (regexec(self->class_compiled, window->class, 0,
				(regmatch_t *)NULL, 0) != 0)
	{
		return NULL;
	}

	if (regexec(self->title_compiled, window->title, 0,
				(regmatch_t *)NULL, 0) != 0)
	{
		return NULL;
	}

	assert(self->gesture_count);

	int g = 0;

	for (g = 0; g < self->gesture_count; ++g)
	{

		Gesture *gest = self->gesture_list[g];

		assert(gest);
		assert(gest->movement);
		assert(gest->movement->expression_compiled);

		if (regexec(gest->movement->expression_compiled, captured_sequence,
					0, (regmatch_t *)NULL, 0) == 0)
		{

			match = gest;
			break;
		}
	}

	return match;
}

Gesture *configuration_process_gesture(Context *self, Capture *grab)
{

	assert(self);
	assert(grab);

	Gesture *gest = NULL;

	int i = 0;

	for (i = 0; i < grab->expression_count; ++i)
	{

		char *sequence = grab->expression_list[i];
		gest = match_gesture(self, sequence, grab->active_window_info);

		if (gest)
		{
			return gest;
		}
	}

	return NULL;
}

Movement *configuration_find_movement_by_name(Context *self,
											  char *movement_name)
{

	assert(self);
	assert(movement_name);

	Movement *m = NULL;

	int i = 0;

	for (i = 0; i < self->movement_count; ++i)
	{
		m = self->movement_list[i];

		if ((m->name) && (movement_name) && (strcasecmp(movement_name, m->name) == 0))
		{
			return m;
		}
	}

	if (self->parent)
	{
		m = configuration_find_movement_by_name(self->parent, movement_name);
		if (m)
		{
			return m;
		}
	}

	return NULL;
}
