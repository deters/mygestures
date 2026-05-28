/*
 Copyright 2026 Lucas Augusto Deters

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <yaml.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "config.h"
#include "actions.h"
#include "configuration.h"
#include "configuration_parser.h"
#include "logging.h"

static char *get_config_dir() {
	char *env = getenv("XDG_CONFIG_HOME");
	char *dir = NULL;
	if (env) {
		dir = strdup(env);
	} else {
		char *home = getenv("HOME");
		int bytes = asprintf(&dir, "%s/.config/mygestures", home);
	}
	assert(dir);
	return dir;
}

static const char *get_environment_suffix(void) {
	const char *swaysock = getenv("SWAYSOCK");
	if (swaysock) return "sway";
	const char *hyprland_sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
	if (hyprland_sig) return "hyprland";
	const char *desktop = getenv("XDG_CURRENT_DESKTOP");
	if (desktop) {
		if (strstr(desktop, "GNOME") || strstr(desktop, "gnome") || strstr(desktop, "Ubuntu")) return "gnome";
		if (strstr(desktop, "KDE") || strstr(desktop, "kde")) return "kde";
	}
	return NULL;
}

char *configuration_get_default_filename() {
	char *dir = get_config_dir();
	const char *suffix = get_environment_suffix();
	char *filename = NULL;
	if (suffix) {
		asprintf(&filename, "%s/mygestures_%s.yaml", dir, suffix);
	} else {
		asprintf(&filename, "%s/mygestures.yaml", dir);
	}
	free(dir);
	assert(filename);
	return filename;
}

static char *yaml_get_template_filename() {
	char *template_file = NULL;
	asprintf(&template_file, "%s/mygestures.yaml", SYSCONFDIR);
	return template_file;
}

static void parse_yaml_movements(yaml_parser_t *parser, Configuration *conf) {
	yaml_event_t event;
	char *key = NULL;

	while (1) {
		yaml_parser_parse(parser, &event);
		if (event.type == YAML_MAPPING_END_EVENT) break;
		if (event.type == YAML_SCALAR_EVENT) {
			if (!key) {
				key = strdup((char *)event.data.scalar.value);
			} else {
				configuration_create_movement(conf, key, strdup((char *)event.data.scalar.value));
				key = NULL;
			}
		}
		yaml_event_delete(&event);
	}
	if (key) free(key);
}

static void parse_yaml_actions(yaml_parser_t *parser, Gesture *gest) {
	yaml_event_t event;
	yaml_parser_parse(parser, &event);
	if (event.type == YAML_SCALAR_EVENT) {
		configuration_add_action_from_string(gest, (char *)event.data.scalar.value);
	} else if (event.type == YAML_SEQUENCE_START_EVENT) {
		while (1) {
			yaml_parser_parse(parser, &event);
			if (event.type == YAML_SEQUENCE_END_EVENT) break;
			if (event.type == YAML_SCALAR_EVENT) {
				configuration_add_action_from_string(gest, (char *)event.data.scalar.value);
			}
			yaml_event_delete(&event);
		}
	}
	yaml_event_delete(&event);
}

static void parse_yaml_gesture(yaml_parser_t *parser, Context *ctx, char *name) {
	yaml_event_t event;
	char *move = NULL;
	Gesture *gest = NULL;

	while (1) {
		yaml_parser_parse(parser, &event);
		if (event.type == YAML_MAPPING_END_EVENT) break;
		if (event.type == YAML_SCALAR_EVENT) {
			char *key = (char *)event.data.scalar.value;
			if (strcmp(key, "move") == 0) {
				yaml_event_delete(&event);
				yaml_parser_parse(parser, &event);
				move = strdup((char *)event.data.scalar.value);
				gest = configuration_create_gesture(ctx, strdup(name), move);
			} else if (strcmp(key, "do") == 0) {
				if (gest) parse_yaml_actions(parser, gest);
				else {
					// Handle actions if move hasn't been parsed yet? 
					// For simplicity, we expect 'move' first or we'll have to buffer.
					// Let's just consume it for now or assume move is always first in our proposal.
				}
			}
		}
		yaml_event_delete(&event);
	}
}

static void parse_yaml_gestures_block(yaml_parser_t *parser, Context *ctx) {
	yaml_event_t event;
	while (1) {
		yaml_parser_parse(parser, &event);
		if (event.type == YAML_MAPPING_END_EVENT) break;
		if (event.type == YAML_SCALAR_EVENT) {
			char *name = strdup((char *)event.data.scalar.value);
			yaml_event_delete(&event);
			yaml_parser_parse(parser, &event); // Start of gesture mapping
			parse_yaml_gesture(parser, ctx, name);
			free(name);
		}
		yaml_event_delete(&event);
	}
}

static void parse_yaml_apps(yaml_parser_t *parser, Configuration *conf) {
	yaml_event_t event;
	while (1) {
		yaml_parser_parse(parser, &event);
		if (event.type == YAML_SEQUENCE_END_EVENT) break;
		if (event.type == YAML_MAPPING_START_EVENT) {
			char *name = NULL;
			char *class = strdup(".*");
			char *title = strdup(".*");
			Context *ctx = NULL;

			while (1) {
				yaml_event_delete(&event);
				yaml_parser_parse(parser, &event);
				if (event.type == YAML_MAPPING_END_EVENT) break;
				char *key = (char *)event.data.scalar.value;
				yaml_event_delete(&event);
				yaml_parser_parse(parser, &event);
				if (strcmp(key, "name") == 0) {
					name = strdup((char *)event.data.scalar.value);
				} else if (strcmp(key, "match") == 0) {
					// Parse match map { class: ..., title: ... }
					while (1) {
						yaml_event_delete(&event);
						yaml_parser_parse(parser, &event);
						if (event.type == YAML_MAPPING_END_EVENT) break;
						char *mkey = (char *)event.data.scalar.value;
						yaml_event_delete(&event);
						yaml_parser_parse(parser, &event);
						if (strcmp(mkey, "class") == 0) {
							free(class);
							class = strdup((char *)event.data.scalar.value);
						} else if (strcmp(mkey, "title") == 0) {
							free(title);
							title = strdup((char *)event.data.scalar.value);
						}
					}
				} else if (strcmp(key, "gestures") == 0) {
					if (!ctx) ctx = configuration_create_context(conf, name ? name : strdup("app"), title, class);
					parse_yaml_gestures_block(parser, ctx);
				}
			}
		}
		yaml_event_delete(&event);
	}
}

static int configuration_parse_file(Configuration *conf, char *filename) {
	FILE *fh = fopen(filename, "r");
	if (!fh) return 1;

	yaml_parser_t parser;
	yaml_event_t event;

	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, fh);

	while (1) {
		yaml_parser_parse(&parser, &event);
		if (event.type == YAML_STREAM_END_EVENT) break;
		if (event.type == YAML_SCALAR_EVENT) {
			char *key = (char *)event.data.scalar.value;
			if (strcmp(key, "movements") == 0) {
				yaml_event_delete(&event);
				yaml_parser_parse(&parser, &event);
				parse_yaml_movements(&parser, conf);
			} else if (strcmp(key, "global") == 0) {
				yaml_event_delete(&event);
				yaml_parser_parse(&parser, &event);
				Context *ctx = configuration_create_context(conf, strdup("global"), strdup(".*"), strdup(".*"));
				parse_yaml_gestures_block(&parser, ctx);
			} else if (strcmp(key, "apps") == 0) {
				yaml_event_delete(&event);
				yaml_parser_parse(&parser, &event);
				parse_yaml_apps(&parser, conf);
			}
		}
		yaml_event_delete(&event);
	}

	yaml_parser_delete(&parser);
	fclose(fh);
	return 0;
}

static int file_copy(const char *from, const char *to) {
	int fd_to, fd_from;
	char buf[4096];
	ssize_t nread;
	fd_from = open(from, O_RDONLY);
	if (fd_from < 0) return -1;
	fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (fd_to < 0) { close(fd_from); return -1; }
	while (nread = read(fd_from, buf, sizeof buf), nread > 0) {
		char *out_ptr = buf;
		ssize_t nwritten;
		do {
			nwritten = write(fd_to, out_ptr, nread);
			if (nwritten >= 0) { nread -= nwritten; out_ptr += nwritten; }
			else if (errno != EINTR) { close(fd_from); close(fd_to); return -1; }
		} while (nread > 0);
	}
	close(fd_from);
	close(fd_to);
	return 0;
}

void configuration_load_from_defaults(Configuration *configuration, int create_config) {
	char *config_file = configuration_get_default_filename();
	FILE *f = fopen(config_file, "r");

	if (!f) {
		char *template = NULL;
		const char *suffix = get_environment_suffix();
		if (suffix) {
			asprintf(&template, "%s/mygestures_%s.yaml", SYSCONFDIR, suffix);
			if (access(template, R_OK) != 0) { free(template); template = NULL; }
		}
		if (!template) template = yaml_get_template_filename();

		if (create_config) {
			char *dir = get_config_dir();
			mkdir(dir, 0700);
			free(dir);
			if (file_copy(template, config_file) == 0) {
				printf("Created default configuration file at '%s'.\n", config_file);
			}
		} else {
			printf("Using internal default configuration from '%s'.\n", template);
			configuration_parse_file(configuration, template);
			free(template); free(config_file);
			return;
		}
		free(template);
	} else {
		fclose(f);
	}

	if (configuration_parse_file(configuration, config_file) != 0) {
		fprintf(stderr, "Error loading configuration from '%s'\n", config_file);
	} else {
		printf("Loaded configuration from file '%s'.\n", config_file);
	}
	free(config_file);
}

void configuration_load_from_file(Configuration *configuration, char *filename) {
	if (configuration_parse_file(configuration, filename) == 0) {
		printf("Loaded %i gestures from '%s'.\n", configuration_get_gestures_count(configuration), filename);
	} else {
		printf("Error loading custom configuration from '%s'\n", filename);
	}
}
