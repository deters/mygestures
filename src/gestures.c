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
 GNU General Public License for more details.

 */

#if HAVE_CONFIG_H          
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <assert.h>

#include "gestures.h"

/* alloc a window struct */
Context *engine_create_context(Engine * self, char * context_name, char *window_title, char *window_class) {
	Context *ans = malloc(sizeof(Context));
	bzero(ans, sizeof(Context));

	ans->name = context_name;
	ans->title = window_title;
	ans->class = window_class;
	ans->engine = self;

	regex_t * title_compiled = NULL;
	if (ans->title) {

		title_compiled = malloc(sizeof(regex_t));
		if (regcomp(title_compiled, window_title,
		REG_EXTENDED | REG_NOSUB)) {
			fprintf(stderr, "Error compiling regexp: %s\n", window_title);
			free(title_compiled);
			title_compiled = NULL;
		}
	}
	ans->title_compiled = title_compiled;

	regex_t * class_compiled = NULL;
	if (ans->class) {
		class_compiled = malloc(sizeof(regex_t));
		if (regcomp(class_compiled, window_class,
		REG_EXTENDED | REG_NOSUB)) {
			fprintf(stderr, "Error compiling regexp: %s\n", window_class);
			class_compiled = NULL;
		}
	}
	ans->class_compiled = class_compiled;

	ans->gestures = malloc(sizeof(Gesture) * 255);

	self->context_list[self->context_count++] = ans;

	return ans;
}

/* release a window struct */
void free_context(Context *free_me) {
	free(free_me->title);
	free(free_me->class);
	free(free_me);
	return;
}

/* alloc a movement struct */
Movement *engine_create_movement(Engine * self, char *movement_name, char *movement_expression) {

	Movement * ans = malloc(sizeof(Movement));
	bzero(ans, sizeof(Movement));

	ans->name = movement_name;
	ans->expression = movement_expression;

	char * regex_str = malloc(sizeof(char) * (strlen(movement_expression) + 5));

	strcpy(regex_str, "");
	strcat(regex_str, "^(");
	strcat(regex_str, movement_expression);
	strcat(regex_str, ")$");

	regex_t * movement_compiled = NULL;
	movement_compiled = malloc(sizeof(regex_t));
	if (regcomp(movement_compiled, regex_str,
	REG_EXTENDED | REG_NOSUB) != 0) {
		fprintf(stderr, "Warning: Invalid movement sequence: %s\n", regex_str);
		free(movement_compiled);
		movement_compiled = NULL;
	} else {
		regcomp(movement_compiled, regex_str,
		REG_EXTENDED | REG_NOSUB);
	}
	free(regex_str);

	ans->compiled = movement_compiled;

	self->movement_list[self->movement_count] = ans;
	self->movement_count++;

	return ans;
}

/* release a movement struct */
void free_movement(Movement *free_me) {
	free(free_me->name);
	free(free_me->expression);
	//free(free_me->movement_compiled);
	free(free_me);
	return;
}

Gesture * context_create_gesture(Context * self, char * gesture_name, char * gesture_movement) {

	Gesture *ans = malloc(sizeof(Gesture));
	bzero(ans, sizeof(Gesture));

	Movement *m = NULL;

	ans->name = gesture_name;
	ans->movement = engine_find_movement_by_name(self->engine, gesture_movement);
	ans->context = self;
	ans->actions_count = 0;
	ans->actions = malloc(sizeof(Action) * 20);

	self->gestures[self->gestures_count++] = ans;

	return ans;
}

//todo gerenciamento de memória
/* release a gesture struct */
void free_gesture(Gesture *free_me) {
	free(free_me->actions);
	free(free_me);

	return;
}

/* alloc an action struct */
Action *gesture_create_action(Gesture * self, int action_type, char * original_str) {

	Action *ans = malloc(sizeof(Action));
	bzero(ans, sizeof(Action));

	ans->type = action_type;
	ans->original_str = original_str;

	self->actions[self->actions_count++] = ans;

	return ans;
}

/* release an action struct */
void free_action(Action *free_me) {
	free(free_me);
	return;
}

Gesture * engine_match_gesture(Engine * self, char * captured_sequence, Window_info * window) {

	Gesture * matched_gesture = NULL;

	int c = 0;

	for (c = 0; c < self->context_count; ++c) {

		if (matched_gesture)
			break;

		Context * context = self->context_list[c];

		if ((!context->class)
				|| (regexec(context->class_compiled, window->class, 0, (regmatch_t *) NULL, 0) != 0)) {
			continue;
		}

		if ((!context->title)
				|| (regexec(context->title_compiled, window->title, 0, (regmatch_t *) NULL, 0))
						!= 0) {
			continue;
		}

		if (context->gestures_count) {

			int g = 0;

			for (g = 0; g < context->gestures_count; ++g) {

				Gesture * gest = context->gestures[g];

				if (gest->movement) {

					if ((gest->movement->compiled)
							&& (regexec(gest->movement->compiled, captured_sequence, 0,
									(regmatch_t *) NULL, 0) == 0)) {

						matched_gesture = gest;
						break;

					}

				}

			}

		}

	}

	return matched_gesture;
}

Gesture * engine_process_gesture(Engine * self, Grabbed * grab) {

	Gesture *gest = NULL;

	int i = 0;

	for (i = 0; i < grab->sequences_count; ++i) {

		char * sequence = grab->sequences[i];
		gest = engine_match_gesture(self, sequence, grab->focused_window);

		if (gest) {
			return gest;
		}

	}

	return NULL;

}

Movement * engine_find_movement_by_name(Engine * self, char * movement_name) {

	Context * ctx;

	if (!movement_name) {
		return NULL;
	}

	int i = 0;

	for (i = 0; i < self->movement_count; ++i) {
		Movement * m = self->movement_list[i];

		if ((m->name) && (movement_name) && (strcasecmp(movement_name, m->name) == 0)) {
			return m;
		}
	}

	return NULL;

}

int engine_get_gestures_count(Engine * self){

	int count = 0;

	for (int c = 0; c < self->context_count; ++c) {
		count += self->context_list[c]->gestures_count;
	}

	return count;

}

Engine * engine_new() {

	Engine * self = malloc(sizeof(Engine));
	bzero(self, sizeof(Engine));

	self->movement_count = 0;
	self->movement_list = malloc(sizeof(Movement *) * 254);

	self->context_count = 0;
	self->context_list = malloc(sizeof(Context *) * 254);

	return self;

}
