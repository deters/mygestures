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

	configuration_create_gesture(context,
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

	configuration_create_movement(eng, movement_name, movement_strokes);
}

static Context *xml_parse_context(xmlNode *node, Context *eng)
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
			context_name = value;
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

	Context *ctx = configuration_create_context(eng, context_name,
												window_title, window_class);

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

			else
			{
				printf("unknown tag '%s' at line %d\n", element,
					   cur_node->line);
			}
		}
	}

	return ctx;
}

// void xml_parse_root(xmlNode *node, Context *eng)
// {

// 	assert(node);
// 	assert(eng);

// 	xmlNode *cur_node = NULL;

// 	int gestures_count = 0;
// 	int contexts_count = 0;

// 	for (cur_node = node->children; cur_node; cur_node = cur_node->next)
// 	{
// 		if (cur_node->type == XML_ELEMENT_NODE)
// 		{

// 			char *element = (char *)cur_node->name;

// 			if (strcasecmp(element, "movement") == 0)
// 			{

// 				xml_parse_movement(cur_node, eng);
// 			}
// 			else if (strcasecmp(element, "context") == 0)
// 			{

// 				Context *ctx = xml_parse_context(cur_node, eng);
// 				gestures_count += ctx->gesture_count;
// 				contexts_count += 1;
// 			}
// 			else
// 			{
// 				printf("unknown tag '%s' at line %d\n", element,
// 					   cur_node->line);
// 			}
// 		}
// 	}
// }

Context *context_parse_file(char *filename)
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

static char *get_config_dir()
{
	char *dir = NULL;

	dir = getenv("XDG_CONFIG_HOME");
	if (!dir)
	{
		char *home = getenv("HOME");
		int size = asprintf(&dir, "%s/.config/mygestures", home);
		if (size < 0)
		{
			printf("Error in asprintf at get_config_dir\n");
			exit(1);
		}
	}

	assert(dir);

	return dir;
}

char *xml_get_template_filename()
{
	char *template_file; // = malloc(sizeof(char) * 4096);
	int size = asprintf(&template_file, "%s/mygestures.xml", SYSCONFDIR);

	if (size < 0)
	{
		printf("Error in asprintf at xml_get_template_filename\n");
		exit(1);
	}

	return template_file;
}

int file_copy(const char *from, const char *to)
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

		do
		{
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

void test_create_dir(char *dir)
{
	struct stat st = {0};
	if (stat(dir, &st) == -1)
	{
		mkdir(dir, 0700);
		perror("in createdir");
	}
}

char *context_get_default_filename()
{
	char *dir = get_config_dir();

	char *filename = NULL;
	int size = asprintf(&filename, "%s/mygestures.xml", dir);

	if (size < 0)
	{
		printf("Error in asprintf at context_get_default_filename\n");
		exit(1);
	}

	assert(filename);
	return filename;
}

Context *configuration_load_from_defaults()
{

	int err = 0;

	char *dir = get_config_dir();
	test_create_dir(dir);

	char *config_file = context_get_default_filename();

	FILE *f = fopen(config_file, "r");

	if (!f)
	{
		char *template = xml_get_template_filename();
		err = file_copy(template, config_file);
		if (err)
		{
			fprintf(stderr,
					"Error creating default context on '%s' from '%s'\n",
					config_file, template);
			return NULL;
		}
	}
	else
	{
		fclose(f);
	}

	Context *root = context_parse_file(config_file);

	if (root)
	{
		fprintf(stderr, "Error loading context from file \n'%s'\n\n",
				config_file);
	}

	printf("Loaded context from file '%s'.\n", config_file);

	return root;
}

Context *configuration_load_from_file(char *filename)
{

	int err = 0;

	Context *root = context_parse_file(
		filename);

	if (err)
	{
		printf("Error loading custom context from '%s'\n", filename);
		return NULL;
	}

	printf("Loaded configuration from \n'%s\n with %d gestures'.\n",
		   filename, root->gesture_count);

	return root;
}
