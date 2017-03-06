/*
 Copyright 2015-2016 Lucas Augusto Deters

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

#define _GNU_SOURCE /* needed by asprintf */


#include <string.h>
#include <libxml/tree.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


#include "assert.h"

#include "config.h"
#include "actions.h"
#include "configuration_parser.h"

const char * CONFIG_FILE_NAME = "mygestures.xml";

void xml_parse_action(xmlNode *node, Gesture * gest) {

	assert(node);
	assert(gest);

	Action * action = NULL;

	char * action_name = NULL;
	char * action_value = NULL;

	xmlAttr* attribute = node->properties;

	while (attribute && attribute->name && attribute->children) {

		char * name = (char *) attribute->name;
		char * value = (char *) xmlNodeListGetString(node->doc, attribute->children, 1);

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
		printf("Missing action name at line %d\n", node->line);
		return;
	}

	int id = ACTION_NULL;

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
	} else if (strcasecmp(action_name, "restore") == 0) {
		id = ACTION_RESTORE;
	} else if (strcasecmp(action_name, "toggle-maximized") == 0) {
		id = ACTION_TOGGLE_MAXIMIZED;
	} else if (strcasecmp(action_name, "keypress") == 0) {
		id = ACTION_KEYPRESS;
	} else if (strcasecmp(action_name, "exec") == 0) {
		id = ACTION_EXECUTE;
	} else {
		printf("unknown action '%s' at line %d\n", action_name, node->line);
		free(action_name);
		free(action_value);
		return;
	}

	if (!action_value) {
		action_value = "";
	}

	configuration_create_action(gest, id, action_value);

}

static Gesture * xml_parse_gesture(xmlNode *node, Context * context) {

	assert(node);
	assert(context);

	char * gesture_name = NULL;
	char * gesture_movement = NULL;

	xmlAttr* attribute = node->properties;
	while (attribute && attribute->name && attribute->children) {

		char * name = (char *) attribute->name;
		char * value = (char *) xmlNodeListGetString(node->doc, attribute->children, 1);

		if (strcasecmp(name, "name") == 0) {
			gesture_name = strdup(value);
		} else if (strcasecmp(name, "movement") == 0) {
			gesture_movement = strdup(value);
		}
		xmlFree(value);
		attribute = attribute->next;
	}

	if (!gesture_name) {
		printf("missing gesture name at line %d\n", node->line);
		free(gesture_movement);
		return NULL;
	}

	if (!gesture_movement) {
		printf("missing gesture movement at line %d\n", node->line);
		free(gesture_name);
		return NULL;
	}

	Gesture * gest = configuration_create_gesture(context, gesture_name, gesture_movement);

	xmlNode *cur_node = NULL;

	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {

			char * element = (char *) cur_node->name;

			if (strcasecmp(element, "do") == 0) {

				xml_parse_action(cur_node, gest);

			} else {
				printf("unknown tag '%s' at line %d\n", element, cur_node->line);
			}

		}
	}

	return gest;

}

static Context * xml_parse_context(xmlNode *node, Configuration * eng) {

	char * context_name = NULL;
	char * window_title = NULL;
	char * window_class = NULL;

	xmlAttr* attribute = node->properties;
	while (attribute && attribute->name && attribute->children) {
		char * name = (char *) attribute->name;
		char * value = (char *) xmlNodeListGetString(node->doc, attribute->children, 1);

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
		printf("Missing context name\n");
		free(window_title);
		free(window_class);
		return NULL;
	}

	if (!window_class) {
		window_class = "";
	}

	if (!window_title) {
		window_title = "";
	}

	Context * ctx = configuration_create_context(eng, context_name, window_title, window_class);

	/* now process the gestures */

	xmlNode *cur_node = NULL;

	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {

			char * element = (char *) cur_node->name;

			if (strcasecmp(element, "gesture") == 0) {

				Gesture * gest = xml_parse_gesture(cur_node, ctx);

			} else {
				printf("unknown tag '%s' at line %d\n", element, cur_node->line);
			}
		}

	}

	return ctx;

}

void xml_parse_movement(xmlNode *node, Configuration * eng) {

	assert(node);
	assert(eng);

	//xmlNode *cur_node = NULL;

	char * movement_name = NULL;
	char * movement_strokes = NULL;

	Movement * movement = NULL;

	xmlAttr* attribute = node->properties;
	while (attribute && attribute->name && attribute->children) {

		char * name = (char *) attribute->name;
		char * value = (char *) xmlNodeListGetString(node->doc, attribute->children, 1);

		if (strcasecmp(name, "name") == 0) {
			movement_name = strdup(value);
		} else if (strcasecmp(name, "value") == 0) {
			movement_strokes = strdup(value);
		}
		xmlFree(value);
		attribute = attribute->next;
	}

	if (!movement_name) {
		printf("missing movement name at line %d\n", node->line);
		free(movement_strokes);
		return;
	}

	if (!movement_strokes) {
		printf("missing movement value at line %d\n", node->line);
		free(movement_name);
		return;
	}

	configuration_create_movement(eng, movement_name, movement_strokes);

}

void xml_parse_root(xmlNode *node, Configuration * eng) {

	assert(node);
	assert(eng);

	xmlNode *cur_node = NULL;

	int gestures_count = 0;
	int contexts_count = 0;

	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {

			char * element = (char *) cur_node->name;

			if (strcasecmp(element, "movement") == 0) {

				xml_parse_movement(cur_node, eng);

			} else if (strcasecmp(element, "context") == 0) {

				Context * ctx = xml_parse_context(cur_node, eng);
				gestures_count += ctx->gesture_count;
				contexts_count += 1;

			} else {
				printf("unknown tag '%s' at line %d\n", element, cur_node->line);
			}

		}

	}

}

static int xml_parse_file(Configuration * conf, char * filename) {
	int result = 0;

	xmlDocPtr doc = NULL;
	xmlNode *root_element = NULL;

	doc = xmlParseFile(filename);

	if (!doc) {
		perror("Empty file.\n");
		return 1;
	}

	root_element = xmlDocGetRootElement(doc);
	xml_parse_root(root_element, conf);

	xmlFreeDoc(doc);
	xmlCleanupParser();

	return 0;

}


static char * xml_get_config_dir(){
	char * dir = NULL;

	dir = getenv("XDG_CONFIG_HOME");
	if (!dir){
		char * home = getenv("HOME");
		int bytes = asprintf(&dir, "%s/.config/mygestures", home);
	}

	assert(dir);

	return dir;
}

char * xml_get_template_filename() {
	char * template_file = malloc(sizeof(char *) * 4096);
	sprintf(template_file, "%s/mygestures.xml", SYSCONFDIR);
	return template_file;
}

int file_copy(const char *from, const char *to) {
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

void test_create_dir(char* dir) {
	struct stat st = { 0 };
	if (stat(dir, &st) == -1) {
		mkdir(dir, 0700);
		perror("in createdir");
	}
}

char* xml_get_default_filename() {
	char* dir = xml_get_config_dir();

	char* filename = NULL;
	int bytes = asprintf(&filename, "%s/mygestures.xml", dir);

	assert(filename);
	return filename;
}

Configuration * xmlconfig_load_engine_from_defaults() {

	Configuration * eng = configuration_new();

	int err = 0;

	char* dir = xml_get_config_dir();
	test_create_dir(dir);

	char* filename = xml_get_default_filename();

	FILE * file = fopen(filename, "r");

	if (!file) {
		char * template = xml_get_template_filename();
		err = file_copy(template, filename);
		if (err) {
			fprintf(stderr, "Error creating default configuration on '%s' from '%s'\n", filename,
					template);
			return NULL;
		}
	} else {
		fclose(file);
	}

	err = xml_parse_file(eng, filename);

	if (err) {
		fprintf(stderr, "Error loading configuration from file \n'%s'\n\n", filename);
	}

	printf("Loaded configuration from file '%s'.\n", filename);

	return eng;

}

Configuration * xml_load_engine_from_file(char * filename) {

	Configuration * eng = configuration_new();

	int err = 0;

	err = xml_parse_file(eng, filename);

	if (err) {
		printf("Error loading custom configuration from '%s'\n", filename);
		return NULL;
	}

	printf("Loaded %i gestures from \n'%s'.\n", configuration_get_gestures_count(eng), filename);

	return eng;

}
