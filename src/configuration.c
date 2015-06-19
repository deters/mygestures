#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "assert.h"

#include "configuration.h"
#include "gestures.h"

char * xml_get_default_filename() {

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

char * xml_get_template_filename() {
	char * template_file = malloc(sizeof(char *) * 4096);
	sprintf(template_file, "%s/mygestures.xml", SYSCONFIR);
	return template_file;
}

Action * xml_parse_action(xmlNode *node, Gesture * gest) {

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
		fprintf(stderr, "Missing action name\n");
		return NULL;
	}

	if (!action_value) {
		action_value = "";
	}

	Action * a = NULL;

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

	a = gesture_create_action(gest, id, action_value);

	return a;

}

Gesture * xml_parse_gesture(xmlNode *node, Context * context) {

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

	Gesture * gest = context_create_gesture(context, gesture_name, gesture_movement);

	xmlNode *cur_node = NULL;

	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {

			char * element = (char *) cur_node->name;

			if (strcasecmp(element, "do") == 0) {

				Action * a = xml_parse_action(cur_node, gest);

			}

		}
	}

	return gest;

}

Context * xml_parse_context(xmlNode *node, Engine * eng) {

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

	Context * ctx = engine_create_context(eng, context_name, window_title, window_class);

	/* now process the gestures */

	xmlNode *cur_node = NULL;

	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {

			char * element = (char *) cur_node->name;

			if (strcasecmp(element, "gesture") == 0) {

				Gesture * gest = xml_parse_gesture(cur_node, ctx);

			}
		}

	}

	return ctx;

}

Movement * xml_parse_movement(xmlNode *node, Engine * eng) {

	xmlNode *cur_node = NULL;

	char * movement_name = NULL;
	char * movement_strokes = NULL;

	Movement * ans = NULL;

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

	if (movement_name && movement_strokes) {

		ans = engine_create_movement(eng, movement_name, movement_strokes);
	}

	return ans;

}

int xml_parse_root(xmlNode *node, Engine * eng) {

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
				gestures_count += ctx->gestures_count;
				contexts_count += 1;

			} else {
				fprintf(stderr, "Expecting only 'movement' or 'context' at root. Ignoring\n");
			}

		}

	}

	//fprintf(stdout, "Loaded %i movements.\n", new_movement_count);
	fprintf(stdout, "Loaded %i gestures in %i contexts.\n", gestures_count, contexts_count);
	return 0;

}

int xml_parse_file(Engine * conf, char * filename) {
	int result = 0;

	xmlDocPtr doc = NULL;
	xmlNode *root_element = NULL;

	doc = xmlParseFile(filename);

	if (!doc) {
		perror("Empty file.\n");
		return 1;
	}

	root_element = xmlDocGetRootElement(doc);
	result = xml_parse_root(root_element, conf);

	xmlFreeDoc(doc);
	xmlCleanupParser();

	return 0;

}

/**
 * Reads the conf file
 */
Engine * xml_engine_load(char * filename) {

	Engine * eng = engine_new();

	int err = 0;

	if (filename) {
		err = xml_parse_file(eng, filename);

		if (err) {
			printf("Error loading custom configuration from '%s'\n", filename);
		}

	} else {

		char * filename = "";

		filename = xml_get_default_filename();
		err = xml_parse_file(eng, filename);

		if (err) {
			printf("Error loading default configuration from '%s'\n", filename);

			filename = xml_get_template_filename();
			err = xml_parse_file(eng, filename);

			if (err) {
				printf("Error loading template configuration from '%s'\n", filename);

			}

		}

	}

	return eng;

}
