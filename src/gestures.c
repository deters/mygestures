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
#include "wm.h"
#include <regex.h>
#include <X11/Xutil.h>
#include <fcntl.h>
#include <errno.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgen.h>
#include <sys/stat.h>

char * conf_file = NULL;

struct movement** movement_list;
int movement_count;

struct context** context_list;
int context_count;

struct action_helper *action_helper;

int init_wm_helper(void) {
	action_helper = &generic_action_helper;

	return 1;
}

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
struct key_press *string_to_keypress(char *str_ptr) {

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
		action_data = string_to_keypress(original_str);
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

struct gesture * gesture_locate(char * captured_sequence,
		struct window_info * window) {

	struct gesture * matched_gesture = NULL;

	int c = 0;

	for (c = 0; c < context_count; ++c) {

		if (matched_gesture)
			break;

		struct context * context = context_list[c];

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

/*
 * Get the parent window.
 *
 * PRIVATE
 */
Window get_parent_window(Display *dpy, Window w) {
	Window root_return, parent_return, *child_return;
	unsigned int nchildren_return;
	int ret;
	ret = XQueryTree(dpy, w, &root_return, &parent_return, &child_return,
			&nchildren_return);

	return parent_return;
}

/*
 * Get the title of a given window at out_window_title.
 *
 * PRIVATE
 */
Status fetch_window_title(Display *dpy, Window w, char **out_window_title) {
	int status;
	XTextProperty text_prop;
	char **list;
	int num;

	status = XGetWMName(dpy, w, &text_prop);
	if (!status || !text_prop.value || !text_prop.nitems) {
		*out_window_title = "";
	}
	status = Xutf8TextPropertyToTextList(dpy, &text_prop, &list, &num);

	if (status < Success || !num || !*list) {
		*out_window_title = "";
	} else {
		*out_window_title = (char *) strdup(*list);
	}
	XFree(text_prop.value);
	XFreeStringList(list);

	return 1;
}

/*
 * Return a window_info struct for the focused window at a given Display.
 *
 * PRIVATE
 */
struct window_info * get_window_info(Display* dpy, Window win) {

	int ret, val;

	char *win_title;
	ret = fetch_window_title(dpy, win, &win_title);

	struct window_info *ans = malloc(sizeof(struct window_info));
	bzero(ans, sizeof(struct window_info));

	char *win_class = NULL;

	XClassHint class_hints;

	int result = XGetClassHint(dpy, win, &class_hints);

	if (class_hints.res_class != NULL)
		win_class = class_hints.res_class;

	if (win_class == NULL) {
		win_class = "";
	}

	if (win_class) {
		ans->class = win_class;
	} else {
		ans->class = "";
	}

	if (win_title) {
		ans->title = win_title;
	} else {
		ans->title = "";
	}

	return ans;

}

/*
 * Return the focused window at the given display.
 *
 * PRIVATE
 */
Window get_focused_window(Display *dpy) {

	Window win = 0;
	int ret, val;
	ret = XGetInputFocus(dpy, &win, &val);

	if (val == RevertToParent) {
		win = get_parent_window(dpy, win);
	}

	return win;

}

void gesture_process_movement(Display * dpy, char ** sequences,
		int sequences_count) {

	struct window_info * focused_window = get_window_info(dpy,
			get_focused_window(dpy));

	struct gesture *gest = NULL;

	printf("\n");
	printf("Window Title = \"%s\"\n", focused_window->title);
	printf("Window Class = \"%s\"\n", focused_window->class);

	int i = 0;

	for (i = 0; i < sequences_count; ++i) {

		char * sequence = sequences[i];

		gest = gesture_locate(sequence, focused_window);

		if (gest) {
			printf(
					"Captured sequence: '%s' --> Movement '%s' --> Gesture '%s'\n",
					sequence, gest->movement->name, gest->name);

			int j = 0;

			for (j = 0; j < gest->actions_count; ++j) {
				struct action * a = gest->actions[j];
				printf(" (%s)\n", a->original_str);
				execute_action(dpy, a, get_focused_window(dpy));
			}

			return;
		} else {
			printf("Captured sequence %s --> not found\n", sequence);
		}
	}

}

/**
 * Execute an action
 */
void execute_action(Display *dpy, struct action *action, Window focused_window) {
	int id;

	// if there is an action
	if (action != NULL) {

		switch (action->type) {
		case ACTION_EXECUTE:
			id = fork();
			if (id == 0) {
				int i = system(action->original_str);
				exit(i);
			}
			if (id < 0) {
				fprintf(stderr, "Error forking.\n");
			}

			break;
		case ACTION_ICONIFY:
			action_helper->iconify(dpy, focused_window);
			break;
		case ACTION_KILL:
			action_helper->kill(dpy, focused_window);
			break;
		case ACTION_RAISE:
			action_helper->raise(dpy, focused_window);
			break;
		case ACTION_LOWER:
			action_helper->lower(dpy, focused_window);
			break;
		case ACTION_MAXIMIZE:
			action_helper->maximize(dpy, focused_window);
			break;
		case ACTION_ROOT_SEND:
			action_helper->root_send(dpy, action->data);
			break;
		default:
			fprintf(stderr, "found an unknown gesture \n");
		}
	}
	return;
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

static void recursive_mkdir(char *path, mode_t mode) {
	char *spath = strdup(path);
	char *next_dir = dirname(spath);

	if (access(next_dir, F_OK) == 0) {
		goto done;
	}

	if (strcmp(next_dir, ".") == 0 || strcmp(next_dir, "/") == 0) {
		goto done;
	}

	recursive_mkdir(next_dir, mode);
	mkdir(next_dir, mode);

	done: free(spath);
	return;
}

/**
 * Copy a file
 */
int new_file_from_template(char *tofile, char *fromfile) {

	recursive_mkdir(tofile, S_IRWXU | S_IRGRP);

	FILE *in, *out;
	char ch;

	if ((in = fopen(fromfile, "rb")) == NULL) {
		fprintf(stderr, "Cannot open input file: %s\n", fromfile);
		return -1;
	}
	if ((out = fopen(tofile, "wb")) == NULL) {
		fprintf(stderr, "Cannot open output file: %s\n", tofile);
		return -2;
	}

	while (!feof(in)) {
		ch = getc(in);
		if (ferror(in)) {
			printf("Read Error");
			clearerr(in);
			break;
		} else {
			if (!feof(in))
				putc(ch, out);
			if (ferror(out)) {
				fprintf(stderr, "Write Error");
				clearerr(out);
				break;
			}
		}
	}
	fclose(in);
	fclose(out);

	return 0;

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
	int x = fread(ret, 1, len, fl);
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

	return a;

}

struct movement * movement_find(char * movement_name,
		struct movement ** known_movements, int known_movements_count) {

	if (!movement_name) {
		return NULL;
	}

	int i = 0;

	for (i = 0; i < known_movements_count; ++i) {
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

	struct context * ctx = alloc_context(context_name, window_title,
			window_class, gestures, gestures_count, abort);

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

	int gestures_count = 0;

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
					gestures_count += ctx->gestures_count;

				}

			} else {
				fprintf(stderr,
						"Expecting only 'movement' or 'context' at root. Ignoring\n");
			}

		}

	}

	fprintf(stdout, "Loaded %i movements.\n", new_movement_count);
	fprintf(stdout, "Loaded %i contexts with %i gestures.\n", new_context_count,
			gestures_count);
	fprintf(stdout,
			"Draw some movement on the screen with the configured button pressed.\n");

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
int gestures_load_from_file(char *filename) {

	int result = 0;

	xmlDocPtr doc = NULL;
	xmlNode *root_element = NULL;

	doc = xmlParseFile(filename);

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

char * gestures_get_default_config() {

	char * filename = malloc(sizeof(char) * 4096);

	char * xdg;

	xdg = getenv("XDG_CONFIG_HOME");

	if (xdg) {
		sprintf(filename, "%s/mygestures/mygestures.xml", xdg);
	} else {
		char * home = getenv("HOME");
		sprintf(filename, "%s/.config/mygestures/mygestures.xml", home);
	}

	return filename;
}

int gestures_init() {

	if (conf_file == NULL) {
		conf_file = gestures_get_default_config();
	}

	FILE *conf = NULL;
	int err = 0;

	conf = fopen(conf_file, "r");

	if (conf) {
		fclose(conf);
	} else {

		char * template_file = malloc(sizeof(char *) * 4096);
		sprintf(template_file, "%s/mygestures.xml", SYSCONFIR);

		err = new_file_from_template(conf_file, template_file);

		if (err) {
			fprintf(stderr,
					"Error trying to create config file `%s' from template `%s'.\n",
					conf_file, template_file);
		} else {
			fprintf(stderr, "Created config file `%s' from template `%s'.\n",
					conf_file, template_file);
		}

		free(template_file);

	}

	fprintf(stdout, "Loading configuration from %s\n", conf_file);

	err = gestures_load_from_file(conf_file);

	if (err) {
		fprintf(stderr, "Error parsing configuration file.\n");
		return err;
	}

	/* choose a wm helper */
	init_wm_helper();

	return 0;
}

