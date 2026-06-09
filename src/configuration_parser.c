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
		if (!home) {
			LOG_ERROR("HOME environment variable not set. Cannot determine configuration directory.\n");
			return NULL;
		}
		if (asprintf(&dir, "%s/.config/mygestures", home) == -1) dir = NULL;
	}
	if (!dir) return NULL;
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
	if (!dir) return NULL;
	const char *suffix = get_environment_suffix();
	char *filename = NULL;
	if (suffix) {
		if (asprintf(&filename, "%s/mygestures_%s.yaml", dir, suffix) == -1) filename = NULL;
	} else {
		if (asprintf(&filename, "%s/mygestures.yaml", dir) == -1) filename = NULL;
	}
	free(dir);
	return filename;
}

static char *yaml_get_template_filename() {
	char *template_file = NULL;
	if (asprintf(&template_file, "%s/mygestures.yaml", SYSCONFDIR) == -1) template_file = NULL;
	return template_file;
}

static void parse_yaml_movements(yaml_parser_t *parser, Configuration *conf) {
	yaml_event_t event;
	char *key = NULL;

	while (1) {
		if (!yaml_parser_parse(parser, &event)) {
			LOG_ERROR("YAML Parser Error: %s at line %lu, column %lu\n", 
				parser->problem, parser->problem_mark.line + 1, parser->problem_mark.column + 1);
			break;
		}
		if (event.type == YAML_MAPPING_END_EVENT) {
			yaml_event_delete(&event);
			break;
		}
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
	if (!yaml_parser_parse(parser, &event)) return;
	if (event.type == YAML_SCALAR_EVENT) {
		configuration_add_action_from_string(gest, (char *)event.data.scalar.value);
	} else if (event.type == YAML_SEQUENCE_START_EVENT) {
		while (1) {
			if (!yaml_parser_parse(parser, &event)) break;
			if (event.type == YAML_SEQUENCE_END_EVENT) {
				yaml_event_delete(&event);
				break;
			}
			if (event.type == YAML_SCALAR_EVENT) {
				configuration_add_action_from_string(gest, (char *)event.data.scalar.value);
			}
			yaml_event_delete(&event);
		}
	}
	yaml_event_delete(&event);
}

static void parse_yaml_gesture(yaml_parser_t *parser, Configuration *conf, char *name) {
	yaml_event_t event;
	char *move = NULL;
	int has_move = 0;
	int has_do = 0;

	Gesture temp_g;
	bzero(&temp_g, sizeof(Gesture));
	temp_g.action_list = malloc(sizeof(Action *) * 20);

	while (1) {
		if (!yaml_parser_parse(parser, &event)) break;
		if (event.type == YAML_MAPPING_END_EVENT) {
			yaml_event_delete(&event);
			break;
		}
		if (event.type == YAML_SCALAR_EVENT) {
			char *key = (char *)event.data.scalar.value;
			if (strcmp(key, "move") == 0) {
				yaml_event_delete(&event);
				if (!yaml_parser_parse(parser, &event)) break;
				if (move) free(move);
				move = strdup((char *)event.data.scalar.value);
				has_move = 1;
			} else if (strcmp(key, "do") == 0) {
				has_do = 1;
				parse_yaml_actions(parser, &temp_g);
			}
		}
		yaml_event_delete(&event);
	}

	Gesture *gest = configuration_find_gesture_by_name(conf, name);
	if (gest) {
		if (conf->parsing_user_config) {
			gest->is_modified = 1;
		}
		if (has_move) {
			if (gest->movement) {
				if (gest->movement->name) free(gest->movement->name);
				if (gest->movement->expression) free(gest->movement->expression);
				if (gest->movement->points) free(gest->movement->points);
				free(gest->movement);
			}
			gest->movement = malloc(sizeof(Movement));
			bzero(gest->movement, sizeof(Movement));
			gest->movement->name = strdup("custom");
			movement_set_expression(gest->movement, strdup(move));
		}
		if (has_do) {
			for (int j = 0; j < gest->action_count; j++) {
				if (gest->action_list[j]->original_str) free(gest->action_list[j]->original_str);
				free(gest->action_list[j]);
			}
			gest->action_count = 0;
			for (int j = 0; j < temp_g.action_count; j++) {
				gest->action_list[gest->action_count++] = temp_g.action_list[j];
			}
			if (temp_g.is_deleted) {
				gest->is_deleted = 1;
			}
		}
	} else {
		char *movement_str = move ? strdup(move) : strdup("0,0 0,0");
		gest = configuration_create_gesture(conf, name, movement_str);
		if (conf->parsing_user_config) {
			gest->is_custom = 1;
		}
		if (has_do) {
			for (int j = 0; j < temp_g.action_count; j++) {
				gest->action_list[gest->action_count++] = temp_g.action_list[j];
			}
			if (temp_g.is_deleted) {
				gest->is_deleted = 1;
			}
		}
	}

	if (move) free(move);
	if (!has_do || gest == NULL) {
		for (int j = 0; j < temp_g.action_count; j++) {
			if (temp_g.action_list[j]->original_str) free(temp_g.action_list[j]->original_str);
			free(temp_g.action_list[j]);
		}
	}
	free(temp_g.action_list);
}

static void parse_yaml_gestures_block(yaml_parser_t *parser, Configuration *conf) {
	yaml_event_t event;
	while (1) {
		if (!yaml_parser_parse(parser, &event)) break;
		if (event.type == YAML_MAPPING_END_EVENT) {
			yaml_event_delete(&event);
			break;
		}
		if (event.type == YAML_SCALAR_EVENT) {
			char *name = strdup((char *)event.data.scalar.value);
			yaml_event_delete(&event);
			if (!yaml_parser_parse(parser, &event)) { free(name); break; }
			parse_yaml_gesture(parser, conf, name);
			free(name);
		}
		yaml_event_delete(&event);
	}
}

static void parse_yaml_apps(yaml_parser_t *parser, Configuration *conf) {
	yaml_event_t event;
	while (1) {
		if (!yaml_parser_parse(parser, &event)) break;
		if (event.type == YAML_SEQUENCE_END_EVENT) {
			yaml_event_delete(&event);
			break;
		}
		if (event.type == YAML_MAPPING_START_EVENT) {
			char *name = NULL;
			char *class = strdup(".*");
			char *title = strdup(".*");

			while (1) {
				yaml_event_delete(&event);
				if (!yaml_parser_parse(parser, &event)) break;
				if (event.type == YAML_MAPPING_END_EVENT) break;
				char *key = (char *)event.data.scalar.value;
				yaml_event_delete(&event);
				if (!yaml_parser_parse(parser, &event)) break;
				if (strcmp(key, "name") == 0) {
					name = strdup((char *)event.data.scalar.value);
				} else if (strcmp(key, "match") == 0) {
					// Parse match map { class: ..., title: ... }
					while (1) {
						yaml_event_delete(&event);
						if (!yaml_parser_parse(parser, &event)) break;
						if (event.type == YAML_MAPPING_END_EVENT) break;
						char *mkey = (char *)event.data.scalar.value;
						yaml_event_delete(&event);
						if (!yaml_parser_parse(parser, &event)) break;
						if (strcmp(mkey, "class") == 0) {
							free(class);
							class = strdup((char *)event.data.scalar.value);
						} else if (strcmp(mkey, "title") == 0) {
							free(title);
							title = strdup((char *)event.data.scalar.value);
						}
					}
				} else if (strcmp(key, "gestures") == 0) {
					parse_yaml_gestures_block(parser, conf);
				}
			}
		}
		yaml_event_delete(&event);
	}
}

static int configuration_parse_file(Configuration *conf, char *filename) {
	FILE *fh = fopen(filename, "r");
	if (!fh) return 1;

	// Quick check for XML format
	char head[10];
	if (fread(head, 1, 5, fh) == 5) {
		if (strncmp(head, "<?xml", 5) == 0 || strncmp(head, "<conf", 5) == 0) {
			LOG_ERROR("File '%s' seems to be in XML format. MyGestures now uses YAML. Please migrate your configuration.\n", filename);
			fclose(fh);
			return 1;
		}
	}
	fseek(fh, 0, SEEK_SET);

	yaml_parser_t parser;
	yaml_event_t event;

	if (!yaml_parser_initialize(&parser)) {
		LOG_ERROR("Failed to initialize YAML parser.\n");
		fclose(fh);
		return 1;
	}
	yaml_parser_set_input_file(&parser, fh);

	while (1) {
		if (!yaml_parser_parse(&parser, &event)) {
			LOG_ERROR("YAML Parser Error in '%s': %s at line %lu, column %lu\n", 
				filename, parser.problem, parser.problem_mark.line + 1, parser.problem_mark.column + 1);
			break;
		}
		if (event.type == YAML_STREAM_END_EVENT) {
			yaml_event_delete(&event);
			break;
		}
		if (event.type == YAML_SCALAR_EVENT) {
			char *key = (char *)event.data.scalar.value;
			if (strcmp(key, "movements") == 0) {
				yaml_event_delete(&event);
				if (yaml_parser_parse(&parser, &event)) parse_yaml_movements(&parser, conf);
			} else if (strcmp(key, "global") == 0) {
				yaml_event_delete(&event);
				if (yaml_parser_parse(&parser, &event)) {
					parse_yaml_gestures_block(&parser, conf);
				}
			} else if (strcmp(key, "apps") == 0) {
				yaml_event_delete(&event);
				if (yaml_parser_parse(&parser, &event)) parse_yaml_apps(&parser, conf);
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

void configuration_save_to_file(Configuration *conf, char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("Failed to open config for writing");
        return;
    }

    fprintf(f, "# MyGestures Configuration - Generated by Gestos\n\n");
    
    int has_overrides = 0;
    for (int j = 0; j < conf->gesture_count; j++) {
        Gesture *g = conf->gesture_list[j];
        if (g->is_custom || g->is_modified || g->is_deleted) {
            has_overrides = 1;
            break;
        }
    }

    if (has_overrides) {
        fprintf(f, "global:\n");
        for (int j = 0; j < conf->gesture_count; j++) {
            Gesture *g = conf->gesture_list[j];
            if (g->is_custom || g->is_modified || g->is_deleted) {
                fprintf(f, "  \"%s\":\n", g->name);
                if (g->is_deleted) {
                    fprintf(f, "    do: disabled\n");
                } else {
                    fprintf(f, "    move: \"%s\"\n", (g->movement && g->movement->expression) ? g->movement->expression : "");
                    if (g->action_count > 0) {
                        Action *a = g->action_list[0];
                        const char *prefix = action_get_prefix(a->type);
                        if (a->original_str && strlen(a->original_str) > 0) {
                            fprintf(f, "    do: %s %s\n", prefix, a->original_str);
                        } else {
                            fprintf(f, "    do: %s\n", prefix);
                        }
                    }
                }
            }
        }
        fprintf(f, "\n");
    }

    fclose(f);
}

void configuration_load_from_defaults(Configuration * configuration, int create_config) {
	char *template = NULL;
	const char *suffix = get_environment_suffix();
	if (suffix) {
		if (asprintf(&template, "%s/mygestures_%s.yaml", SYSCONFDIR, suffix) == -1) template = NULL;
		if (template && access(template, R_OK) != 0) { free(template); template = NULL; }
	}
	if (!template) template = yaml_get_template_filename();

	if (template && access(template, R_OK) == 0) {
		printf("Loading system defaults from '%s'...\n", template);
		configuration->parsing_user_config = 0;
		if (configuration_parse_file(configuration, template) != 0) {
			fprintf(stderr, "Warning: Failed to load system defaults from '%s'\n", template);
		}
	} else {
		fprintf(stderr, "Warning: System defaults template not found at '%s'\n", template ? template : "unknown");
	}
	if (template) free(template);

	char *config_file = configuration_get_default_filename();
	if (config_file) {
		if (access(config_file, R_OK) == 0) {
			printf("Loading user overrides from '%s'...\n", config_file);
			configuration->parsing_user_config = 1;
			if (configuration_parse_file(configuration, config_file) != 0) {
				fprintf(stderr, "Warning: Failed to load user overrides from '%s'\n", config_file);
			}
		} else {
			printf("User configuration '%s' not found. Operating with system defaults.\n", config_file);
		}
		free(config_file);
	}
}

void configuration_load_from_file(Configuration *configuration, char *filename) {
	if (configuration_parse_file(configuration, filename) == 0) {
		printf("Loaded %i gestures from '%s'.\n", configuration_get_gestures_count(configuration), filename);
	} else {
		printf("Error loading custom configuration from '%s'\n", filename);
	}
}
