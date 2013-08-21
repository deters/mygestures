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
#include <strings.h>
#include <string.h>
#include "gestures.h"
#include "helpers.h"
#include "wm.h"
#include <regex.h>
#include <X11/Xutil.h>
#include <fcntl.h>
#include <errno.h>

#define TEMPLATE_FILE "/usr/share/mygestures/mygestures.conf"


/* The name from the actions, to read in .gestures file. */
char *action_names[] = { "NONE", "exit", "exec", "minimize", "kill", "reconf",
		"raise", "lower", "maximize", "root_send" };

/* this holds all known gestures */
struct gesture **specific_gestures = NULL;
int specific_gestures_count = 0;
/* a temporary stack - it remains necessary?) */
EMPTY_STACK(temp_stack);

/* a temporary stack - it remains necessary?) */
EMPTY_STACK(movement_stack);

/* this holds all known gestures */
struct gesture **global_gestures = NULL;
int global_gestures_count = 0;
/* a temporary stack - it remains necessary?) */
EMPTY_STACK(temp_general_stack);

/* alloc a window struct */
struct context *alloc_context(char *window_title, char *window_class,
		regex_t window_title_compiled, regex_t window_class_compiled) {
	struct context *ans = malloc(sizeof(struct context));
	bzero(ans, sizeof(struct context));

	ans->title = window_title;
	ans->class = window_class;
	ans->title_compiled = window_title_compiled;
	ans->class_compiled = window_class_compiled;
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
struct movement *alloc_movement(char *movement_name, char *movement_expression,
		regex_t movement_compiled) {
	struct movement *ans = malloc(sizeof(struct movement));
	bzero(ans, sizeof(struct movement));

	ans->name = movement_name;
	ans->expression = movement_expression;
	ans->compiled = movement_compiled;
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

/* alloc a gesture struct */
struct gesture *alloc_gesture(struct context *context,
		struct action *gesture_action, struct movement *gesture_movement) {
	struct gesture *ans = malloc(sizeof(struct gesture));
	bzero(ans, sizeof(struct gesture));

	ans->context = context;
	ans->action = gesture_action;
	ans->movement = gesture_movement;
	return ans;
}

/* release a gesture struct */
void free_gesture(struct gesture *free_me) {
	free(free_me->action);
	free(free_me);

	return;
}

/* alloc an action struct */
struct action *alloc_action(int action_type, void *action_data, char * original_str) {
	struct action *ans = malloc(sizeof(struct action));
	bzero(ans, sizeof(struct action));
	ans->type = action_type;
	ans->data = action_data;
	ans->original_str = original_str;

	return ans;
}

/* release an action struct */
void free_action(struct action *free_me) {
	free(free_me);
	return;
}

/* alloc a key_press struct ???? */
struct key_press * alloc_key_press(void) {
	struct key_press *ans = malloc(sizeof(struct key_press));
	bzero(ans, sizeof(struct key_press));
	return ans;
}

/* release a key_press struct */
void free_key_press(struct key_press *free_me) {
	free(free_me);
	return;
}

/**
 * Creates a Keysym from a char sequence
 *
 * PRIVATE
 */
void * compile_key_action(char *str_ptr) {
	struct key_press base;
	struct key_press *key;
	KeySym k;
	char *str = str_ptr;
	char *token = str;
	char *str_dup;

	if (str == NULL)
		return NULL;

	/* do this before strsep.. */
	str_dup = strdup(str);

	key = &base;
	token = strsep(&str_ptr, "+\n ");
	while (token != NULL) {
		/* printf("found : %s\n", token); */
		k = XStringToKeysym(token);
		if (k == NoSymbol) {
			fprintf(stderr, "error converting %s to keysym\n", token);
			exit(-1);
		}
		key->next = (struct key_press *) alloc_key_press();
		key = key->next;
		key->key = k;
		token = strsep(&str_ptr, "+\n ");
	}

	base.next->original_str = str_dup;

	return base.next;
}

/*
 * Match the window, the class and the grabbed gesture with a known gesture
 */
int match_gesture(char *stroke_sequence,
		const struct window_info *current_window, struct gesture *known_gesture) {

	// match the stroke_sequence against the current gesture movement
	int status = regexec(&(known_gesture->movement->compiled), stroke_sequence,
			0, (regmatch_t *) NULL, 0);

	if (status == 0) {

		// match current window against gesture context
		if (known_gesture->context->title != NULL) {
			status = regexec(&(known_gesture->context->title_compiled),
					current_window->title, 0, (regmatch_t *) NULL, 0);
		}

		// try to match also the class
		if (known_gesture->context->class != NULL) {
			status = regexec(&(known_gesture->context->class_compiled),
					current_window->class, 0, (regmatch_t *) NULL, 0);
		}

	}

	return status;

}


struct gesture * lookup_gesture(char * captured_sequence,
		struct window_info * current_context, struct gesture **gesture_list,
		int gesture_list_count) {

	struct gesture * gest = NULL;
	int i;

	for (i = 0; i < gesture_list_count; i++) {
		if (!match_gesture(captured_sequence, current_context,
				gesture_list[i])) {

			gest = gesture_list[i];
			break;

		}
	}

	return gest;

}

/**
 * Receives captured movement from two diferent algoritms: a simple and a complex.
 * 
 *   The simple algoritm can understand movements in four directions: U, D, R, L
 *   The complex algoritm can understand movements in eight directions, including the diagonals
 * 
 * The program will process the complex movement sequence prior to the simplest sequence.
 * 
 * The system will also prioritize the gestures defined for the current application, 
 * and only then will consider the global gestures.
 */
struct gesture * process_movement_sequences(Display * dpy,
		struct window_info *current_context, char *complex_sequence,
		char * simple_sequence) {

	struct gesture *gest = NULL;

	printf("\n");
	printf("Captured sequence: %s\n", complex_sequence);
	printf("Simplified       : %s\n", simple_sequence);
	printf("Class            : %s\n", current_context->class);
	printf("Title            : %s\n", current_context->title);

	gest = lookup_gesture(complex_sequence, current_context, specific_gestures,
			specific_gestures_count);

	if (gest == NULL) {
		gest = lookup_gesture(complex_sequence, current_context,
				global_gestures, global_gestures_count);
	}

	if (gest == NULL) {

		gest = lookup_gesture(simple_sequence, current_context,
				specific_gestures, specific_gestures_count);
	}

	if (gest == NULL) {
		gest = lookup_gesture(simple_sequence, current_context, global_gestures,
				global_gestures_count);
	}

	// execute the action
	if (gest == NULL) {

		printf("No gesture matches captured sequences.\n");

		return NULL;

	} else {

		printf("Matched          : %s\n",gest->movement->expression);

		if ((gest->context->class == NULL) && (gest->context->title == NULL)) {
    	printf("Context          : All applications\n");
		} else {
			printf("Context          : Class = %s\n", gest->context->class);
			printf("                   Title = %s\n", gest->context->title);
		}

			if (gest->action != NULL){
				char * str = gest->action->original_str;
				printf("Action           : %s %s\n", action_names[gest->action->type],str );
			} else {
				printf("Null action.\n");
			}


		return gest;

	}

}

/**
 * Removes the line break from a string
 */
char *remove_new_line(char *str) {
	int len = 0;
	int i;
	if (str == NULL)
		return NULL;
	len = strlen(str);

	for (i = 0; i < len; i++)
		if (str[i] == '\n')
			str[i] = '\0';
	return str;

}







/** 
 * Copy a file
 */
int cp(const char *to, const char *from)
{
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_to < 0)
        goto out_error;

    while (nread = read(fd_from, buf, sizeof buf), nread > 0)
    {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

    if (nread == 0)
    {
        if (close(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);

        /* Success! */
        return 0;
    }

  out_error:
    saved_errno = errno;

    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
}






/**
 * Reads the conf file
 */
int read_config(char *conf_file) {

	FILE *conf = fopen(conf_file, "r");

	struct action *action;
	struct gesture *gest;
	struct context *gesture_window;
	char buff[4096];

	struct movement *known_movements[254];
	int known_movements_num = 0;

	char *buff_ptr = buff;
	char **buff_ptr_ptr;
	char *gesture_action;

	char *gesture_params;
	void *data;
	char *sequence;
	int i;
	char *token;
	char *window_title = NULL;
	char *window_class = NULL;

	if (conf == NULL) {
		return -1;
	}

	int currentline = -1;

	// get 1 line
	while (fgets(buff, 4096, conf) != NULL) {

		currentline = currentline + 1;

		// ignore comments
		if (buff[0] == '#')
			continue;

		// remove line breaks
		remove_new_line(buff);

		buff_ptr = buff;
		buff_ptr_ptr = &buff_ptr;

		// ignoring white spaces
		token = strsep(buff_ptr_ptr, " \t");
		while ((token != NULL) && (strcmp(token, "") == 0)) {
			token = strsep(buff_ptr_ptr, " \t");
		}
		if ((token == NULL) || (strlen(token) == 0))
			continue; // go to next line

		// found a TITLE token

		if (strcasecmp(token, "TITLE") == 0) {
			// get the window title
			token = strsep(buff_ptr_ptr, " \t");
			if (token == NULL)
				continue;
			window_class = NULL; // the class remains empty
			window_title = strdup(token); // copy the window title

			regex_t reg;

			if (regcomp(&reg, window_title, REG_EXTENDED | REG_NOSUB) != 0) {
				printf("Error on compiling a regular expression: \t%s\n",
						window_title);
				exit(1); // exit
			}

			gesture_window = alloc_context(window_title, window_class, reg,
					reg/*TODO NULL*/);

			continue; // go to next line
		}

		// found a CLASS token
		if (strcasecmp(token, "CLASS") == 0) {
			// get the window class
			token = strsep(buff_ptr_ptr, " \t");
			if (token == NULL)
				continue;
			window_class = strdup(token);
			window_title = NULL; // the title remain empty

			regex_t reg;

			if (regcomp(&reg, window_class, REG_EXTENDED | REG_NOSUB) != 0) {
				printf("Error on compiling a regular expression: \t%s\n",
						window_class);
				exit(1); // exit
			}

			gesture_window = alloc_context(window_title, window_class,
					reg/*TODO NULL*/, reg);

			continue;
		}

		// found a ALL token
		if (strcasecmp(token, "ALL") == 0) {
			// next lines configuration will be valid to any window
			window_class = NULL;
			window_title = NULL;

			regex_t reg; // TODO: Remove this. will not be used

			gesture_window = alloc_context(window_title, window_class,
					reg/*TODO NULL*/, reg/*TODO NULL*/);

			continue;
		}

		// found a MOVEMENT token
		if (strcasecmp(token, "MOVEMENT") == 0) {
			// get the name of the movement
			token = strsep(buff_ptr_ptr, " \t");
			if (token == NULL)
				continue;
			char *movement_name = malloc(sizeof(char) * strlen(token));
			strcpy(movement_name, token);

			// get the regular expression of the movement
			token = strsep(buff_ptr_ptr, " \t");
			if (token == NULL) {
				free(movement_name);
				continue;
			}

			// forces the start and end of the RE
			char *movement_value = malloc(sizeof(char) * (strlen(token) + 5));
			strcpy(movement_value, "");
			strcat(movement_value, "^(");
			strcat(movement_value, token);
			strcat(movement_value, ")$");

			// compile the regular expression for the movement
			regex_t movement_compiled;
			if (regcomp(&movement_compiled, movement_value,
			REG_EXTENDED | REG_NOSUB) != 0) {
				printf("Warning: Invalid movement sequence: %s\tat line %i",
						movement_value, currentline - 1);
				exit(1); // exit
			}

			struct movement *movement = alloc_movement(movement_name,
					movement_value, movement_compiled);

			known_movements[known_movements_num] = movement;
			known_movements_num = known_movements_num + 1;
			continue;
		}

		// If not found a token, then this line is a GESTURE definition
		// A GESTURE contains a MOVEMENT name, a ACTION name and the PARAMS to the action

		struct movement *gesture_movement;

		char *movementused = token;

		// TODO remove this variables:
		char *mov_name = "";
		char *mov_value = "";

		// Try to get the Movement from the known movements
		// TODO: create a separated method to do this
		for (i = 0; i < known_movements_num; i++) {

			char *movement_name =
					((struct movement *) (known_movements[i]))->name;
			char *movement_value =
					((struct movement *) (known_movements[i]))->expression;

			if (strcmp(movementused, movement_name) == 0) {
				gesture_movement = ((struct movement *) (known_movements[i]));
				// TODO remove this variables
				mov_value = movement_value;
				mov_name = movement_name;
				continue;
			}

		}
		if (gesture_movement == NULL) {
			continue; //ingores the movement.
		}

		// get the ACTION name
		token = strsep(buff_ptr_ptr, " \t");
		if (token == NULL)
			continue;
		gesture_action = token;

		// get the PARAMS of the action
		gesture_params = *buff_ptr_ptr; // the remainder chars on the line

		for (i = 1; i < ACTION_LAST; i++) {
			char * str = NULL;

			if (strncasecmp(action_names[i], token, strlen(action_names[i]))
					!= 0)
				continue;

			// ACTION_EXECUTE
			if (i == ACTION_EXECUTE) {
				// parameters mandatory
				if (strlen(gesture_params) <= 0) {
					fprintf(stderr, "error in exec action\n");
					continue;
				}
				str = strdup(gesture_params);
				data = str;
				// ACTION_ROOT_SEND
			} else if (i == ACTION_ROOT_SEND) {
				// parameters mandatory
				if (strlen(gesture_params) <= 0) {
					fprintf(stderr, "error in exec action\n");
					continue;
				}
				// try to compile the key
				str = strdup(gesture_params);
				data = compile_key_action(strdup(str));
				if (data == NULL) {
					fprintf(stderr, "error reading config file: root_send\n");
					exit(-1);
				}
			} else
				// OTHER ACTIONS DON'T NEED PARAMETERS
				data = NULL;

			// creates an ACTION
			action = alloc_action(i, data, str);

			// creates the gesture
			sequence = strdup(movementused);

			if (strcmp(mov_name, "") != 0) {
				gest = alloc_gesture(gesture_window, action, gesture_movement);
			} else {
				//gest = alloc_gesture(sequence, action, window_title, window_class,
				//"Unknown movement");
				printf("Warning: Invalid gesture name at line %i: %s \n",
						currentline, sequence);
				continue;
			}

			// push the gesture to a temporary stack
			// TODO: the temp_stack remain's necessary??? the is no more sort...
			if ((window_title == NULL) && (window_class == NULL)) {
				push(gest, &temp_general_stack);
			} else {
				push(gest, &temp_stack);
			}

			break;

		}

	}

	// closes the file
	fclose(conf);
	return 0;

}

/**
 * Sort the gesture sequences... // TODO: make a refactoring
 */

int init_gestures(char *config_file) {
	int err = 0;
	int i;

	printf("Loading gestures from %s\n", config_file);

	err = read_config(config_file);

	if (err){
		printf("Creating file '%s' from default config at '%s'\n",
				config_file, TEMPLATE_FILE);

		err = cp(config_file,"/usr/share/mygestures/mygestures.conf");

		printf("Done.\n");
	}

	if (err) {
		printf("Error trying to create config file %s.\n",
						config_file);
		return err;
	}

	err = read_config(config_file);

	if (err){
		printf("Error reading configuration file.\n");
		return err;
	}

	/* now, fill the gesture array */
	if (specific_gestures_count != 0) /* reconfiguration.. */
		free(specific_gestures);

	specific_gestures_count = stack_size(&temp_stack);
	specific_gestures = malloc(
			sizeof(struct gesture *) * specific_gestures_count);

	/* printf("got %d gests\n", known_gestures_num); */

	for (i = 0; i < specific_gestures_count; i++) {
		specific_gestures[i] = (struct gesture *) pop(&temp_stack);
	}

	/* now, fill the gesture array */
	if (global_gestures_count != 0) /* reconfiguration.. */
		free(global_gestures);
	global_gestures_count = stack_size(&temp_general_stack);
	global_gestures = malloc(sizeof(struct gesture *) * global_gestures_count);

	for (i = 0; i < global_gestures_count; i++) {
		global_gestures[i] = (struct gesture *) pop(&temp_general_stack);
	}

	printf("%d gestures loaded.\n", specific_gestures_count + global_gestures_count );

	return err;
}
