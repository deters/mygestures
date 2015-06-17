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

#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gestures.h"
#include "wm.h"
#include <regex.h>
#include <X11/Xutil.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>
#include <assert.h>

/* alloc a window struct */
struct context *engine_create_context(struct engine * self, char * context_name,
		char *window_title, char *window_class) {
	struct context *ans = malloc(sizeof(struct context));
	bzero(ans, sizeof(struct context));

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

	ans->gestures = malloc(sizeof(struct gesture) * 255);

	self->context_list[self->context_count++] = ans;

	return ans;
}

/* release a window struct */
void free_context(struct context *free_me) {
	free(free_me->title);
	free(free_me->class);
	free(free_me);
	return;
}

/* alloc a movement struct */
struct movement *engine_create_movement(struct engine * self,
		char *movement_name, char *movement_expression) {

	struct movement * ans = malloc(sizeof(struct movement));
	bzero(ans, sizeof(struct movement));

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
void free_movement(struct movement *free_me) {
	free(free_me->name);
	free(free_me->expression);
	//free(free_me->movement_compiled);
	free(free_me);
	return;
}

struct gesture * context_create_gesture(struct context * self,
		char * gesture_name, char * gesture_movement) {

	struct gesture *ans = malloc(sizeof(struct gesture));
	bzero(ans, sizeof(struct gesture));

	struct movement *m = NULL;

	ans->name = gesture_name;
	ans->movement = engine_find_movement_by_name(self->engine,
			gesture_movement);
	ans->context = self;
	ans->actions_count = 0;
	ans->actions = malloc(sizeof(struct action) * 20);

	self->gestures[self->gestures_count++] = ans;

	return ans;
}

//todo gerenciamento de memória
/* release a gesture struct */
void free_gesture(struct gesture *free_me) {
	free(free_me->actions);
	free(free_me);

	return;
}

/* alloc an action struct */
struct action *gesture_create_action(struct gesture * self, int action_type,
		char * original_str) {

	struct action *ans = malloc(sizeof(struct action));
	bzero(ans, sizeof(struct action));

	ans->type = action_type;
	ans->original_str = original_str;

	self->actions[self->actions_count++] = ans;

	return ans;
}

/* release an action struct */
void free_action(struct action *free_me) {
	free(free_me);
	return;
}

struct gesture * engine_match_gesture(struct engine * self,
		char * captured_sequence, struct window_info * window) {

	struct gesture * matched_gesture = NULL;

	int c = 0;

	for (c = 0; c < self->context_count; ++c) {

		if (matched_gesture)
			break;

		struct context * context = self->context_list[c];

		if ((!context->class)
				|| (regexec(context->class_compiled, window->class, 0,
						(regmatch_t *) NULL, 0) != 0)) {
			continue;
		}

		if ((!context->title)
				|| (regexec(context->title_compiled, window->title, 0,
						(regmatch_t *) NULL, 0)) != 0) {
			continue;
		}

		if (context->gestures_count) {

			int g = 0;

			for (g = 0; g < context->gestures_count; ++g) {

				struct gesture * gest = context->gestures[g];

				if (gest->movement) {

					if ((gest->movement->compiled)
							&& (regexec(gest->movement->compiled,
									captured_sequence, 0, (regmatch_t *) NULL,
									0) == 0)) {

						matched_gesture = gest;
						break;

					}

				}

			}

		}

	}

	return matched_gesture;
}

struct gesture * engine_process_gesture(struct engine * self,
		struct grabbed * grab) {

	struct gesture *gest = NULL;

	int i = 0;

	for (i = 0; i < grab->sequences_count; ++i) {

		char * sequence = grab->sequences[i];

		gest = engine_match_gesture(self, sequence, grab->focused_window);

		if (gest) {

			printf(
					"Captured sequence: '%s' --> Movement '%s' --> Gesture '%s'\n",
					sequence, gest->movement->name, gest->name);

			return gest;
		}
	}

	return NULL;

}

struct movement * engine_find_movement_by_name(struct engine * self,
		char * movement_name) {

	struct context * ctx;

	if (!movement_name) {
		return NULL;
	}

	int i = 0;

	for (i = 0; i < self->movement_count; ++i) {
		struct movement * m = self->movement_list[i];

		if ((m->name) && (movement_name)
				&& (strcasecmp(movement_name, m->name) == 0)) {
			return m;
		}
	}

	return NULL;

}

struct engine * engine_new() {

	struct engine * self = malloc(sizeof(struct engine));
	bzero(self, sizeof(struct engine));

	self->movement_count = 0;
	self->movement_list = malloc(sizeof(struct movement *) * 254);

	self->context_count = 0;
	self->context_list = malloc(sizeof(struct context *) * 254);

	return self;

}
