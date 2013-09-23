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
#include <jansson.h>
#include <regex.h>
#include <X11/Xutil.h>
#include <fcntl.h>
#include <errno.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifndef TEMPLATE_FILE
#define TEMPLATE_FILE "/usr/share/mygestures/mygestures2.xml"
#endif

char * conf_file = NULL;

struct movement** movement_list;
int movement_count;

struct context** context_list;
int context_count;

/* The name from the actions, to read in .gestures file. */
/*char *action_names[] = { "NONE", "exit", "exec", "minimize", "kill", "reconf",
 "raise", "lower", "maximize", "root_send" };*/

/* alloc a window struct */
struct context *alloc_context(char * context_name, char *window_title,
		char *window_class, struct gesture ** gestures, int gestures_count,
		int abort) {
	struct context *ans = malloc(sizeof(struct context));
	bzero(ans, sizeof(struct context));

	ans->name = context_name;
	ans->title = window_title;
	ans->class = window_class;
	ans->gestures = gestures;
	ans->gestures_count = gestures_count;

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
struct movement *alloc_movement(char *movement_name, char *movement_expression) {
	struct movement *ans = malloc(sizeof(struct movement));
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
struct gesture *alloc_gesture(char * gesture_name,
		struct movement *gesture_movement, struct action **gesture_actions,
		int actions_count) {
	struct gesture *ans = malloc(sizeof(struct gesture));
	bzero(ans, sizeof(struct gesture));

	ans->name = gesture_name;
	ans->movement = gesture_movement;
	ans->actions = gesture_actions;
	ans->actions_count = actions_count;
	return ans;
}

/* alloc a key_press struct ???? */
struct key_press * alloc_key_press(void) {
	struct key_press *ans = malloc(sizeof(struct key_press));
	bzero(ans, sizeof(struct key_press));
	return ans;
}

/**
 * Creates a Keysym from a char sequence
 *
 * PRIVATE
 */
struct key_press *compile_key_action(char *str_ptr) {

	char * copy = strdup(str_ptr);

	struct key_press base;
	struct key_press *key;
	KeySym k;
	char *str = copy;
	char *token = str;
	char *str_dup;

	if (str == NULL)
		return NULL;

	key = &base;
	token = strsep(&copy, "+\n ");
	while (token != NULL) {
		/* printf("found : %s\n", token); */
		k = XStringToKeysym(token);
		if (k == NoSymbol) {
			fprintf(stderr, "error converting %s to keysym\n", token);
			exit(-1);
		}
		key->next = alloc_key_press();
		key = key->next;
		key->key = k;
		token = strsep(&copy, "+\n ");
	}

	base.next->original_str = str_ptr;
	return base.next;
}

//todo gerenciamento de memória
/* release a gesture struct */
void free_gesture(struct gesture *free_me) {
	free(free_me->actions);
	free(free_me);

	return;
}

/* alloc an action struct */
struct action *alloc_action(int action_type, char * original_str) {
	struct action *ans = malloc(sizeof(struct action));
	bzero(ans, sizeof(struct action));

	struct key_press * action_data = NULL;

	if (action_type == ACTION_ROOT_SEND) {
		// TODO: limpar no método free
		action_data = compile_key_action(original_str);
	}

	ans->type = action_type;
	ans->original_str = original_str;
	ans->data = action_data;

	return ans;
}

/* release an action struct */
void free_action(struct action *free_me) {
	free(free_me);
	return;
}

/* release a key_press struct */
void free_key_press(struct key_press *free_me) {
	free(free_me);
	return;
}

struct gesture * match_gestures(char * captured_sequence,
		struct window_info * window) {

	struct gesture * matched_gesture = NULL;

	for (int c = 0; c < context_count; ++c) {

		if (matched_gesture)
			break;

		struct context * context = context_list[c];

		fprintf(stderr, "Context: %s\n", context->name);

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

		fprintf(stderr, "Matched context: %s\n", context->name);

		if (context->gestures_count) {

			fprintf(stderr, "       gesture_count: %d\n",
					context->gestures_count);

			for (int g = 0; g < context->gestures_count; ++g) {

				struct gesture * gest = context->gestures[g];

				if (gest->movement) {

					if ((gest->movement->compiled)
							&& (regexec(gest->movement->compiled,
									captured_sequence, 0, (regmatch_t *) NULL,
									0) == 0)) {

						fprintf(stderr, "          matched movement:  %s %s.\n",
								gest->movement->name,
								gest->movement->expression);

						matched_gesture = gest;
						break;

					}

				}

			}

		}

	}

	return matched_gesture;
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
void process_movement_sequences(Display * dpy,
		struct window_info *current_context, char ** sequences,
		int sequences_count) {

	struct gesture *gest = NULL;

	printf("\n");
	printf("Searching for sequences:");

	for (int i = 0; i < sequences_count; ++i) {

		char * sequence = sequences[i];

		printf("\t%s", sequence);

		gest = match_gestures(sequence, current_context);

		if (gest) {
			break;
		}
	}

	printf("\n");

	if (!gest) {

		printf("No gesture matches captured sequences.\n");
		printf("Window Class = %s\n", current_context->class);
		printf("Window Title = %s\n", current_context->title);

	} else {

		printf("Gesture found    : %s \n", gest->name);
		printf("                   %d actions on gesture. \n",
				gest->actions_count);

	}

	if (gest) {

		for (int i = 0; i < gest->actions_count; ++i) {

			struct action * a = gest->actions[i];
			fprintf(stderr, "   gesture type: %d\n", a->type);
			execute_action(dpy, a);

		}

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
int new_file_from_template(const char *to, const char *from) {
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

	while (nread = read(fd_from, buf, sizeof buf), nread > 0) {
		char *out_ptr = buf;
		ssize_t nwritten;

		do {
			nwritten = write(fd_to, out_ptr, nread);

			if (nwritten >= 0) {
				nread -= nwritten;
				out_ptr += nwritten;
			} else if (errno != EINTR) {
				goto out_error;
			}
		} while (nread > 0);
	}

	if (nread == 0) {
		if (close(fd_to) < 0) {
			fd_to = -1;
			goto out_error;
		}
		close(fd_from);

		/* Success! */
		return 0;
	}

	out_error: saved_errno = errno;

	close(fd_from);
	if (fd_to >= 0)
		close(fd_to);

	errno = saved_errno;
	return -1;
}

char* readFileBytes(const char *name) {
	FILE *fl = fopen(name, "r");

	if (fl == NULL) {
		return NULL;
	}

	fseek(fl, 0, SEEK_END);
	long len = ftell(fl);
	char *ret = malloc(len);
	fseek(fl, 0, SEEK_SET);
	fread(ret, 1, len, fl);
	fclose(fl);
	return ret;
}

struct action * parse_action(xmlNode *node) {

	char * action_name = NULL;
	char * action_value = NULL;

	xmlAttr* attribute = node->properties;
	while (attribute && attribute->name && attribute->children) {

		char * name = (char *) attribute->name;
		char * value = (char *) xmlNodeListGetString(node->doc,
				attribute->children, 1);

		if (strcasecmp(name, "action") == 0) {
			action_name = strdup(value);
		} else if (strcasecmp(name, "value") == 0) {
			action_value = strdup(value);
		}

		xmlFree(value);
		attribute = attribute->next;
	}

	if (!action_name) {
		free(action_value);
		fprintf(stderr, "Missing action name\n");
		return NULL;
	}

	if (!action_value) {
		action_value = "";
	}

	struct action * a = NULL;

	fprintf(stderr, "key = %s\n", action_value);

	int id = ACTION_ERROR;

	if (strcasecmp(action_name, "iconify") == 0) {
		id = ACTION_ICONIFY;
	} else if (strcasecmp(action_name, "kill") == 0) {
		id = ACTION_KILL;
	} else if (strcasecmp(action_name, "lower") == 0) {
		id = ACTION_LOWER;
	} else if (strcasecmp(action_name, "raise") == 0) {
		id = ACTION_RAISE;
	} else if (strcasecmp(action_name, "maximize") == 0) {
		id = ACTION_MAXIMIZE;
	} else if (strcasecmp(action_name, "keypress") == 0) {
		id = ACTION_ROOT_SEND;
	} else if (strcasecmp(action_name, "exec") == 0) {
		id = ACTION_EXECUTE;
	} else {
		fprintf(stderr, "unknown gesture: %s\n", action_name);
	}

	a = alloc_action(id, action_value);

	fprintf(stderr, "ALLOCATED %s from %s\n", a->original_str, action_value);

	return a;

}

struct movement * movement_find(char * movement_name,
		struct movement ** known_movements, int known_movements_count) {

	if (!movement_name) {
		return NULL;
	}

	for (int i = 0; i < known_movements_count; ++i) {
		struct movement * m = known_movements[i];

		if ((m->name) && (movement_name)
				&& (strcasecmp(movement_name, m->name) == 0)) {
			return m;
		}
	}

	return NULL;

}

struct gesture * parse_gesture(xmlNode *node,
		struct movement ** known_movements, int known_movements_count) {

	char * gesture_name = NULL;
	struct movement * gesture_trigger = NULL;

	xmlAttr* attribute = node->properties;
	while (attribute && attribute->name && attribute->children) {

		char * name = (char *) attribute->name;
		char * value = (char *) xmlNodeListGetString(node->doc,
				attribute->children, 1);

		if (strcasecmp(name, "name") == 0) {
			gesture_name = strdup(value);
		} else if (strcasecmp(name, "movement") == 0) {
			gesture_trigger = movement_find(value, known_movements,
					known_movements_count);
		}
		xmlFree(value);
		attribute = attribute->next;
	}

	if (!gesture_name) {
		free(gesture_trigger);
		fprintf(stderr, "Missing gesture name\n");
		return NULL;
	}

	if (!gesture_trigger) {
		free(gesture_name);
		fprintf(stderr, "Missing gesture movement.\n");
		return NULL;
	}

	struct action ** gesture_actions = malloc(sizeof(struct action *) * 254);
	int actions_count = 0;

	xmlNode *cur_node = NULL;

	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {

			char * element = (char *) cur_node->name;

			if (strcasecmp(element, "do") == 0) {

				struct action * a = parse_action(cur_node);

				if (a) {
					gesture_actions[actions_count++] = a;
				}

			}

		}
	}

	if (!actions_count) {
		fprintf(stderr, "Missing actions associated to the gesture %s\n",
				gesture_name);
		/* ignoring */
	}

	fprintf(stderr, "gesture:  %s %s %d\n", gesture_name, gesture_trigger->name,
			actions_count);

	struct gesture * gest = NULL;

	gest = alloc_gesture(gesture_name, gesture_trigger, gesture_actions,
			actions_count);

	return gest;

}

struct context * parse_context(xmlNode *node,
		struct movement ** known_movements, int known_movements_count) {

	char * context_name = NULL;
	char * window_title = NULL;
	char * window_class = NULL;

	xmlAttr* attribute = node->properties;
	while (attribute && attribute->name && attribute->children) {
		char * name = (char *) attribute->name;
		char * value = (char *) xmlNodeListGetString(node->doc,
				attribute->children, 1);

		if (strcasecmp(name, "name") == 0) {
			context_name = strdup(value);
		} else if (strcasecmp(name, "windowtitle") == 0) {
			window_title = strdup(value);
		} else if (strcasecmp(name, "windowclass") == 0) {
			window_class = strdup(value);
		}
		xmlFree(value);
		attribute = attribute->next;
	}

	// TODO: criar o context e só depois ir adicionando os elementos.

	if (!context_name) {
		free(window_title);
		free(window_class);
		fprintf(stderr, "Missing context name\n");
		return NULL;
	}

	if (!window_class) {
		window_class = "";
	}

	if (!window_title) {
		window_title = "";
	}

	/* now process the gestures */

	xmlNode *cur_node = NULL;

	struct gesture ** gestures = malloc(sizeof(struct gesture *) * 254);
	struct gesture * gest = NULL;
	int gestures_count = 0;
	int abort = 0;

	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {

			char * element = (char *) cur_node->name;

			if (strcasecmp(element, "gesture") == 0) {

				gest = parse_gesture(cur_node, known_movements,
						known_movements_count);
				if (gest) {
					gestures[gestures_count++] = gest;
				}

			} else if (strcasecmp(element, "abort") == 0) {
				abort = 1;
			}
		}

	}

	fprintf(stderr, "%s with %d gestures\n", context_name, gestures_count);

	struct context * ctx = alloc_context(context_name, window_title,
			window_class, gestures, gestures_count, abort);

	fprintf(stderr, "%s\n", context_name);

	return ctx;

}

struct movement * parse_movement(xmlNode *node) {

	xmlNode *cur_node = NULL;

	char * movement_name = NULL;
	char * movement_strokes = NULL;

	struct movement * ans = NULL;

	xmlAttr* attribute = node->properties;
	while (attribute && attribute->name && attribute->children) {

		char * name = (char *) attribute->name;
		char * value = (char *) xmlNodeListGetString(node->doc,
				attribute->children, 1);

		if (strcasecmp(name, "name") == 0) {
			movement_name = strdup(value);
		} else if (strcasecmp(name, "value") == 0) {
			movement_strokes = strdup(value);
		}
		xmlFree(value);
		attribute = attribute->next;
	}

	if (movement_name && movement_strokes) {
		ans = alloc_movement(movement_name, movement_strokes);
	}

	return ans;

}

int parse_root(xmlNode *node) {

	xmlNode *cur_node = NULL;

	struct movement ** new_movement_list = malloc(
			sizeof(struct movement *) * 254);
	int new_movement_count = 0;

	struct context ** new_context_list = malloc(sizeof(struct context *) * 254);
	int new_context_count = 0;

	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {

			char * element = (char *) cur_node->name;

			if (strcasecmp(element, "movement") == 0) {

				struct movement * m = parse_movement(cur_node);
				if (m) {
					new_movement_list[new_movement_count++] = m;
				}

				// found a "context" token
			} else if (strcasecmp(element, "context") == 0) {

				struct context * ctx = parse_context(cur_node,
						new_movement_list, new_movement_count);
				if (ctx) {
					new_context_list[new_context_count++] = ctx;

				}

			} else {
				fprintf(stderr,
						"Expecting only 'movement' or 'context' at root. Ignoring\n");
			}

			printf("node type: Element, name: %s\n", cur_node->name);
		}

	}

	fprintf(stderr, "Loaded %i movements.\n", new_movement_count);
	fprintf(stderr, "Loaded %i contexts.\n", new_context_count);

	// update global variables

	movement_list = new_movement_list;
	movement_count = new_movement_count;

	context_list = new_context_list;
	context_count = new_context_count;

	return 0;

}

/**
 * Reads the conf file
 */
int parse_config_file(char *conf) {

	int result = 0;

	xmlDocPtr doc = NULL;
	xmlNode *root_element = NULL;

	doc = xmlParseFile(conf);

	if (!doc) {
		return 1;
	}

	root_element = xmlDocGetRootElement(doc);
	result = parse_root(root_element);

	xmlFreeDoc(doc);
	xmlCleanupParser();

	return result;

}

void gestures_set_config_file(char * config_file) {
	if (conf_file) {
		free(conf_file);
	}
	conf_file = strdup(config_file);
}

char * get_user_conf_file() {
	char * home = getenv("HOME");
	char * filename = malloc(sizeof(char) * 4096);
	strncpy(filename, strdup(home), 4096);
	strncat(filename, "/.config/mygestures/mygestures2.xml", 4096);
	return filename;
}

int gestures_init() {

	if (conf_file == NULL) {
		conf_file = get_user_conf_file();
	}

	FILE *conf = NULL;
	int err = 0;

	conf = fopen(conf_file, "r");

	if (conf) {
		fclose(conf);
	} else {

		err = new_file_from_template(conf_file,
		TEMPLATE_FILE);

		if (err) {
			fprintf(stderr,
					"Error trying to create config file `%s' from template `%s'.\n",
					conf_file, TEMPLATE_FILE);
			return err;
		}

	}

	err = parse_config_file(conf_file);

	if (err) {
		printf("Error parsing configuration file %s.\n", conf_file);
		return err;
	}

	return 0;
}

