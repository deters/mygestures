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

#include "configuration_parser.h"

const char *CONFIG_FILE_NAME = "mygestures.xml";

static void xml_parse_gesture(xmlNode *node, Context *context)
{

	assert(node);
	assert(context);

	char *gesture_movement = NULL;
	char *command = NULL;

	xmlAttr *attribute = node->properties;
	while (attribute && attribute->name && attribute->children)
	{

		char *name = (char *)attribute->name;
		char *value = (char *)xmlNodeListGetString(node->doc,
												   attribute->children, 1);

		if (strcasecmp(name, "movement") == 0)
		{
			gesture_movement = value;
		}

		if (strcasecmp(name, "command") == 0)
		{
			command = value;
		}

		attribute = attribute->next;
	}

	if (!command)
	{
		printf("missing gesture command at line %d\n", node->line);
		return;
	}

	if (!gesture_movement)
	{
		printf("missing gesture movement at line %d\n", node->line);
		return;
	}

	context_create_gesture(context,
						   gesture_movement,
						   command);
}

void xml_parse_movement(xmlNode *node, Context *eng)
{

	assert(node);
	assert(eng);

	//xmlNode *cur_node = NULL;

	char *movement_name = NULL;
	char *movement_strokes = NULL;

	xmlAttr *attribute = node->properties;
	while (attribute && attribute->name && attribute->children)
	{

		char *name = (char *)attribute->name;
		char *value = (char *)xmlNodeListGetString(node->doc,
												   attribute->children, 1);

		if (strcasecmp(name, "name") == 0)
		{
			movement_name = value;
		}
		else if (strcasecmp(name, "value") == 0)
		{
			movement_strokes = value;
		}
		//xmlFree(value);
		attribute = attribute->next;
	}

	if (!movement_name)
	{
		printf("missing movement name at line %d\n", node->line);
		return;
	}

	if (!movement_strokes)
	{
		printf("missing movement value at line %d\n", node->line);
		return;
	}

	context_create_movement(eng, movement_name, movement_strokes);
}

static Context *xml_parse_context(xmlNode *node, Context *parent)
{

	char *context_name = "";
	char *window_title = "";
	char *window_class = "";

	xmlAttr *attribute = node->properties;
	while (attribute && attribute->name && attribute->children)
	{
		char *name = (char *)attribute->name;
		char *value = (char *)xmlNodeListGetString(node->doc,
												   attribute->children, 1);

		if (strcasecmp(name, "name") == 0)
		{
			if (!parent)
			{
				context_name = "root";
			}
			else
			{
				context_name = value;
			}
		}
		else if (strcasecmp(name, "windowtitle") == 0)
		{
			window_title = value;
		}
		else if (strcasecmp(name, "windowclass") == 0)
		{
			window_class = value;
		}
		//xmlFree(value);
		attribute = attribute->next;
	}

	Context *ctx = context_create_context(parent, context_name,
										  window_title, window_class);

	if (parent)
	{
		parent->context_list[parent->context_count++] = ctx;
	}

	/* now process the gestures */

	xmlNode *cur_node = NULL;

	for (cur_node = node->children; cur_node; cur_node = cur_node->next)
	{
		if (cur_node->type == XML_ELEMENT_NODE)
		{

			char *element = (char *)cur_node->name;

			if (strcasecmp(element, "gesture") == 0)
			{

				xml_parse_gesture(cur_node, ctx);
			}
			else if (strcasecmp(element, "movement") == 0)
			{
				xml_parse_movement(cur_node, ctx);
			}
			else if (strcasecmp(element, "context") == 0)
			{
				xml_parse_context(cur_node, ctx);
			}
			else
			{
				printf("unknown tag '%s' at line %d\n", element,
					   cur_node->line);
			}
		}
	}

	return ctx;
}

Context *xml_open_file(char *filename)
{
	xmlDocPtr doc = NULL;
	xmlNode *root_element = NULL;

	doc = xmlParseFile(filename);

	if (!doc)
	{
		perror("Empty file.\n");
		return NULL;
	}

	root_element = xmlDocGetRootElement(doc);
	Context *root = xml_parse_context(root_element, NULL);

	xmlFreeDoc(doc);
	xmlCleanupParser();

	return root;
}

static char *get_user_configuration()
{
	char *home = getenv("XDG_CONFIG_HOME");

	char *filename = NULL;

	if (!filename)
	{
		home = getenv("HOME");
	}

	int size = asprintf(&filename, "%s/.config/mygestures/mygestures.xml", home);
	if (size < 0)
	{
		printf("Error in asprintf at get_user_configuration\n");
		exit(1);
	}

	assert(filename);

	return filename;
}

static char *get_system_configuration()
{

	char *filename = NULL;

	int size = asprintf(&filename, "%s/mygestures.xml", SYSCONFDIR);
	if (size < 0)
	{
		printf("Error in asprintf at get_system_configuration\n");
		exit(1);
	}

	assert(filename);

	return filename;
}

Context *configuration_load_from_defaults()
{

	char *config_file = get_user_configuration();

	FILE *f = fopen(config_file, "r");

	if (!f)
	{
		printf("%s NOT FOUND. Ignoring\n", config_file);

		config_file = get_system_configuration();
		f = fopen(config_file, "r");
	}

	if (!f)
	{
		printf("%s NOT FOUND. EXITING\n", config_file);
		exit(1);
	}
	else
	{
		fclose(f);
	}

	Context *root = xml_open_file(config_file);

	if (!root)
	{
		fprintf(stderr, "Error loading context from file \n'%s'\n\n",
				config_file);
		return NULL;
	}

	printf("Loaded configuration from %s\n", config_file);

	return root;
}

Context *configuration_load_from_file(char *filename)
{

	int err = 0;

	Context *root = xml_open_file(
		filename);

	if (err)
	{
		printf("Error loading custom context from '%s'\n", filename);
		return NULL;
	}

	printf("Loaded configuration from %s\n",
		   filename);

	return root;
}
