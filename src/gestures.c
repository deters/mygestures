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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE /* needed by asprintf */
#include <stdio.h>  /* needed by asprintf */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <regex.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgen.h>
#include <sys/stat.h>
#include <assert.h>

#include "gestures.h"

/* alloc a gesture struct */
static struct gesture *_alloc_gesture(struct context * context,
		char * gesture_name, struct movement *gesture_movement,
		struct action **gesture_actions, int actions_count) {

	assert(gesture_name);
	assert(gesture_movement);
	assert(gesture_actions);
	assert(context);
	assert(actions_count >= 0);

	struct gesture *ans = malloc(sizeof(struct gesture));
	bzero(ans, sizeof(struct gesture));

	ans->context = context;
	ans->name = gesture_name;
	ans->movement = gesture_movement;
	ans->actions = gesture_actions;
	ans->actions_count = actions_count;
	return ans;
}

/* release a movement struct */
static void _free_movement(struct movement *free_me) {

	assert(free_me);

	free(free_me->name);
	free(free_me->expression);
	regfree(free_me->compiled);
	free(free_me->compiled);
	free(free_me);
	return;
}

/* release an action struct */
static void _free_action(struct action *free_me) {

	assert(free_me);

	free(free_me->data);
	free(free_me);
	return;
}

/* release a gesture struct */
static void _free_gesture(struct gesture *free_me) {

	assert(free_me);

	free(free_me->name);

	//free_movement(free_me->movement);
	//free_context(free_me->context);

	int i = 0;

	for (; i < free_me->actions_count; ++i) {
		_free_action(free_me->actions[i]);
	}

	free(free_me);

	return;
}

static void _free_context(struct context * free_me) {
	assert(free_me);

	int i = 0;

	for (; i < free_me->context_count; ++i) {
		_free_context(free_me->context_list[i]);
	}

	for (; i < free_me->gestures_count; ++i) {
		_free_gesture(free_me->gestures[i]);
	}

	free(free_me->name);
	free(free_me->title);
	free(free_me->class);
	regfree(free_me->class_compiled);
	free(free_me->class_compiled);
	regfree(free_me->title_compiled);
	free(free_me->title_compiled);

	i = 0;
	for (; i < free_me->movement_count; ++i) {
		_free_movement(free_me->movements[i]);
	}

	i = 0;
	for (; i < free_me->gestures_count; ++i) {
		_free_gesture(free_me->gestures[i]);
	}
	free(free_me);

}

/* alloc a window struct */
static struct context *alloc_context(char * context_name, char *window_title,
		char *window_class, struct context * parent) {

	assert(context_name);
	assert(window_title);
	assert(window_class);

	struct context *ans = malloc(sizeof(struct context));
	bzero(ans, sizeof(struct context));

	ans->name = context_name;
	ans->title = window_title;
	ans->class = window_class;

	ans->context_list = malloc(sizeof(struct context *) * 254);
	ans->context_count = 0;

	ans->gestures = malloc(sizeof(struct gesture *) * 254);
	ans->gestures_count = 0;

	ans->movements = malloc(sizeof(struct movement *) * 254);
	ans->movement_count = 0;

	// TODO: GET ERROR DETAILS WITH regerror

	regex_t * title_compiled = NULL;
	if (ans->title) {

		title_compiled = malloc(sizeof(regex_t));
		if (regcomp(title_compiled, window_title,
		REG_EXTENDED | REG_NOSUB)) {
			fprintf(stderr, "Error compiling regexp: %s\n", window_title);
			regfree(title_compiled);
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
			regfree(class_compiled);
			class_compiled = NULL;
		}
	}
	ans->class_compiled = class_compiled;

	ans->parent = parent;

	return ans;
}

/* alloc a movement struct */
static struct movement *alloc_movement(char *movement_name,
		char *movement_expression) {

	assert(movement_name);
	assert(movement_expression);

	struct movement *ans = malloc(sizeof(struct movement));
	bzero(ans, sizeof(struct movement));

	ans->name = movement_name;
	ans->expression = movement_expression;

	char * regex_str = NULL;
	asprintf(&regex_str, "^(%s)$", movement_expression);

	regex_t * movement_compiled = NULL;
	movement_compiled = malloc(sizeof(regex_t));
	if (regcomp(movement_compiled, regex_str,
	REG_EXTENDED | REG_NOSUB) != 0) {
		fprintf(stderr, "Warning: Invalid movement sequence: %s\n", regex_str);
	}
	free(regex_str);

	ans->compiled = movement_compiled;

	return ans;
}

/* alloc an action struct */
static struct action *alloc_action(int action_type, char * action_value) {

	assert(action_type >= 0);

	struct action * ans = malloc(sizeof(struct action));

	bzero(ans, sizeof(struct action));

	ans->type = action_type;
	ans->data = action_value;

	return ans;
}

struct gesture * context_match_gesture(struct context * context,
		char * captured_sequence, char * window_class, char * window_title) {

	if (context == NULL) {
		return NULL;
	}
	int c = 0;

	struct gesture * res = NULL;

	for (c = 0; c < context->context_count; ++c) {
		res = context_match_gesture(context->context_list[c], captured_sequence,
				window_class, window_title);

		if (res) {
			return res;
		}
	}

	if ((!context->class)
			|| (regexec(context->class_compiled, window_class, 0,
					(regmatch_t *) NULL, 0) != 0)) {
		return NULL;
	}

	if ((!context->title)
			|| (regexec(context->title_compiled, window_title, 0,
					(regmatch_t *) NULL, 0)) != 0) {
		return NULL;
	}

	if (context->gestures_count) {

		int g = 0;

		for (g = 0; g < context->gestures_count; ++g) {

			struct gesture * gest = context->gestures[g];

			if (gest->movement) {

				if ((gest->movement->compiled)
						&& (regexec(gest->movement->compiled, captured_sequence,
								0, (regmatch_t *) NULL, 0) == 0)) {

					res = gest;
					break;

				}

			}

		}

	}

	return res;

}

struct gesture * config_match_captured(config * conf, capture * captured) {

	assert(conf);
	assert(captured);

	struct gesture * matched = NULL;

	struct context * root_context = conf->root_context;

	int i = 0;
	for (; i < captured->movement_representations_count; ++i) {
		matched = context_match_gesture(root_context,
				captured->movement_representations[i], captured->window_class,
				captured->window_title);
		if (matched) {
			break;
		}
	}

	return matched;

}

static void _recursive_mkdir(char *path, mode_t mode) {

	assert(path);

	char *spath = strdup(path);
	char *next_dir = dirname(spath);

	if (access(next_dir, F_OK) == 0) {
		goto done;
	}

	if (strcmp(next_dir, ".") == 0 || strcmp(next_dir, "/") == 0) {
		goto done;
	}

	_recursive_mkdir(next_dir, mode);
	mkdir(next_dir, mode);

	done: free(spath);
	return;
}

static struct action * _parse_action(xmlNode *node) {

	assert(node);

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

	struct action * a = NULL;

	if (!action_name) {
		fprintf(stderr, "Missing action name\n");

		free(action_value);

	} else {

		if (!action_value) {
			action_value = "";
		}

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
			fprintf(stderr, "unknown action: %s\n", action_name);
		}

		a = alloc_action(id, action_value);

		free(action_name);

	}

	return a;

}

static struct movement * context_find_movement(struct context * context,
		char * movement_name) {

	assert(movement_name);
	assert(context);

	if (!movement_name) {
		return NULL;
	}

	int i = 0;

	for (i = 0; i < context->movement_count; ++i) {
		struct movement * m = context->movements[i];

		if ((m->name) && (movement_name)
				&& (strcasecmp(movement_name, m->name) == 0)) {
			return m;
		}
	}

	if (context->parent) {
		return context_find_movement(context->parent, movement_name);
	}

	return NULL;

}

static struct gesture * _parse_gesture(xmlNode *node, struct context * context) {

	assert(node);
	assert(context);

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
			gesture_trigger = context_find_movement(context, value);
		}
		xmlFree(value);
		attribute = attribute->next;
	}

	if (!gesture_name) {
		//free(gesture_trigger);
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

				struct action * a = _parse_action(cur_node);

				if (a) {
					gesture_actions[actions_count++] = a;
				}

			}

		}
	}

	if (!actions_count) {
		fprintf(stderr,
				"Alert: No actions associated to the gesture %s. Ignoring.\n",
				gesture_name);
	}

	struct gesture * gest = NULL;

	gest = _alloc_gesture(context, gesture_name, gesture_trigger,
			gesture_actions, actions_count);

	return gest;

}

static struct movement * _parse_movement(xmlNode *node) {

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

static struct context * _parse_context(xmlNode *node, struct context * parent) {

	assert(node);

	char * context_name = NULL;
	char * window_title = NULL;
	char * window_class = NULL;

	if (parent) {

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

	} else {
		context_name = "Any Application";
	}

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

	// parse node items (context gestures)

	xmlNode *cur_node = NULL;

	struct context * ctx = alloc_context(context_name, window_title,
			window_class, parent);

	struct gesture * gest = NULL;

	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {

			char * element = (char *) cur_node->name;

			if (strcasecmp(element, "movement") == 0) {

				struct movement * m = _parse_movement(cur_node);
				if (m) {
					ctx->movements[ctx->movement_count] = m;
					ctx->movement_count = ctx->movement_count + 1;
				}

				// found another "context" token
			} else if (strcasecmp(element, "context") == 0) {

				struct context * subcontext = _parse_context(cur_node, ctx);
				if (subcontext) {
					ctx->context_list[ctx->context_count] = subcontext;
					ctx->context_count = ctx->context_count + 1;

				}

			} else if (strcasecmp(element, "gesture") == 0) {

				gest = _parse_gesture(cur_node, ctx);
				if (gest) {
					ctx->gestures[ctx->gestures_count] = gest;
					ctx->gestures_count = ctx->gestures_count + 1;
				}

			} else {
				fprintf(stderr, "Unknown token: %s\n", element);
			}

		}

	}

	return ctx;

}

static int _file_copy(char * target, char * source) {

	int err = CONFIG_OK;

	if (access(target, F_OK) != -1) {
		// file already exists. do not overwrite.
		return -1;
	}

	_recursive_mkdir(target, S_IRWXU | S_IRGRP);

	FILE *in, *out;
	char ch;

	if ((in = fopen(source, "rb")) == NULL) {
		fprintf(stderr, "Cannot open input file: %s\n", source);
		return CONFIG_CREATE_ERROR;
	}
	if ((out = fopen(target, "wb")) == NULL) {
		fprintf(stderr, "Cannot open output file: %s\n", target);
		return CONFIG_CREATE_ERROR;
	}

	while (!feof(in)) {
		ch = getc(in);
		if (ferror(in)) {
			printf("Read Error\n");
			clearerr(in);
			break;
		} else {
			if (!feof(in))
				putc(ch, out);
			if (ferror(out)) {
				fprintf(stderr, "Write Error\n");
				clearerr(out);
				break;
			}
		}
	}
	fclose(in);
	fclose(out);

	return err;

}

static struct context * _parse_root_node(config * conf, xmlNode *node) {

	assert(conf);
	assert(node);

	struct context * root_context = _parse_context(node, NULL);

	return root_context;

}

void config_free(config* conf) {

	if (conf) {

		if (conf->root_context) {
			_free_context(conf->root_context);
		}

		if (conf->config_file) {
			free(conf->config_file);
		}
	}

	return;
}

/**
 * Reads the conf file
 */
int config_load_from_file(config * c, char *filename) {

	assert(c);
	assert(filename);

	xmlDocPtr doc = NULL;
	xmlNode *root_node = NULL;

	doc = xmlParseFile(filename);

	if (!doc) {
		if (access(filename, F_OK) == -1) {
			return CONFIG_FILE_NOT_FOUND;
		} else {
			return CONFIG_PARSE_ERROR;
		}
	}

	/* If already loaded, clear current root_context */
	if (c->root_context) {
		_free_context(c->root_context);
	}

	/* load root context */
	root_node = xmlDocGetRootElement(doc);
	c->root_context = _parse_root_node(c, root_node);

	if (c->config_file) {
		free(c->config_file);
	}
	c->config_file = filename;

	xmlFreeDoc(doc);
	xmlCleanupParser();

	return CONFIG_OK;

}

char * config_get_filename(config * conf) {

	assert(conf);

	char * result = NULL;
	asprintf(&result, conf->config_file);
	return result;
}

config * config_new() {

	config * conf = malloc(sizeof(config));

	conf->config_file = NULL;
	conf->root_context = NULL;

	return conf;

}

char * config_get_template_filename() {
	char * template_filename = NULL;
	asprintf(&template_filename, "%s/mygestures.xml", SYSCONFIR);
	return template_filename;
}

char * config_get_default_filename() {

	char * default_filename = NULL;

	char * xdg = getenv("XDG_CONFIG_HOME"); // return of getenv must not be modified

	if (xdg) {
		asprintf(&default_filename, "%s/mygestures/mygestures.xml", xdg);
	} else {
		char * home = getenv("HOME");   // return of getenv must not be modified
		if (home) {
			asprintf(&default_filename, "%s/.config/mygestures/mygestures.xml",
					home);
		} else {
			asprintf(&default_filename, ".config/mygestures/mygestures.xml");
		}
	}

	return default_filename;

}

int config_create_from_default(config * conf) {

	assert(conf);

	char * target_filename = config_get_default_filename();

	char * template_filename = config_get_template_filename();

	int err = _file_copy(target_filename, template_filename);

	free(template_filename);
	free(target_filename);

	return err;

}

/*
 * Load configuration from default place
 */
int config_load_from_default(config * conf) {

	assert(conf);

	/* try to load from default */
	char * default_filename = config_get_default_filename();
	int err = config_load_from_file(conf, default_filename);

	free(default_filename);

	return err;

}
