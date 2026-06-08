/*
 Copyright 2026 Lucas Augusto Deters
 Copyright 2005 Nir Tzachar

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>

#include "wayland.h"
#include "actions.h"
#include "configuration.h"
#include "logging.h"
#include "wayland_context.h"

typedef struct JsonNode {
	char *name;
	char *app_id;
	char *class;
	int focused;
} JsonNode;

static void skip_space(const char **ptr)
{
	while (**ptr && (**ptr == ' ' || **ptr == '\t' || **ptr == '\r' || **ptr == '\n'))
	{
		(*ptr)++;
	}
}

static char *parse_json_string(const char **ptr)
{
	if (**ptr != '"') return NULL;
	(*ptr)++; // skip open quote
	const char *start = *ptr;
	while (**ptr && **ptr != '"')
	{
		if (**ptr == '\\' && *(*ptr + 1))
		{
			(*ptr)++; // skip escape char
		}
		(*ptr)++;
	}
	size_t len = *ptr - start;
	char *str = malloc(len + 1);
	if (str)
	{
		memcpy(str, start, len);
		str[len] = '\0';
	}
	if (**ptr == '"') (*ptr)++; // skip close quote
	return str;
}

static void parse_json_val(const char **ptr, JsonNode *focused_node, JsonNode *current_node);

static void parse_json_object(const char **ptr, JsonNode *focused_node, JsonNode *current_node)
{
	if (**ptr != '{') return;
	(*ptr)++; // skip '{'

	JsonNode local_node;
	memset(&local_node, 0, sizeof(local_node));

	while (**ptr)
	{
		skip_space(ptr);
		if (**ptr == '}')
		{
			(*ptr)++; // skip '}'
			break;
		}

		char *key = parse_json_string(ptr);
		if (!key) break;

		skip_space(ptr);
		if (**ptr == ':') (*ptr)++; // skip ':'
		skip_space(ptr);

		if (strcmp(key, "focused") == 0)
		{
			if (strncmp(*ptr, "true", 4) == 0)
			{
				local_node.focused = 1;
				*ptr += 4;
			}
			else if (strncmp(*ptr, "false", 5) == 0)
			{
				local_node.focused = 0;
				*ptr += 5;
			}
			else
			{
				parse_json_val(ptr, focused_node, &local_node);
			}
		}
		else if (strcmp(key, "name") == 0)
		{
			local_node.name = parse_json_string(ptr);
			if (!local_node.name)
			{
				parse_json_val(ptr, focused_node, &local_node);
			}
		}
		else if (strcmp(key, "app_id") == 0)
		{
			local_node.app_id = parse_json_string(ptr);
			if (!local_node.app_id)
			{
				parse_json_val(ptr, focused_node, &local_node);
			}
		}
		else if (strcmp(key, "window_properties") == 0)
		{
			JsonNode sub_node;
			memset(&sub_node, 0, sizeof(sub_node));
			parse_json_val(ptr, focused_node, &sub_node);
			if (sub_node.class)
			{
				local_node.class = strdup(sub_node.class);
			}
			else if (sub_node.name)
			{
				local_node.class = strdup(sub_node.name);
			}

			if (sub_node.class) free(sub_node.class);
			if (sub_node.name) free(sub_node.name);
		}
		else if (strcmp(key, "class") == 0)
		{
			local_node.class = parse_json_string(ptr);
			if (!local_node.class)
			{
				parse_json_val(ptr, focused_node, &local_node);
			}
		}
		else
		{
			parse_json_val(ptr, focused_node, &local_node);
		}

		free(key);

		skip_space(ptr);
		if (**ptr == ',')
		{
			(*ptr)++; // skip ','
		}
	}

	if (local_node.focused)
	{
		if (focused_node->name) free(focused_node->name);
		if (focused_node->app_id) free(focused_node->app_id);
		if (focused_node->class) free(focused_node->class);

		focused_node->focused = 1;
		focused_node->name = local_node.name ? strdup(local_node.name) : NULL;
		focused_node->app_id = local_node.app_id ? strdup(local_node.app_id) : NULL;
		focused_node->class = local_node.class ? strdup(local_node.class) : NULL;
	}

	if (current_node)
	{
		if (local_node.name && !current_node->name)
			current_node->name = strdup(local_node.name);
		if (local_node.app_id && !current_node->app_id)
			current_node->app_id = strdup(local_node.app_id);
		if (local_node.class && !current_node->class)
			current_node->class = strdup(local_node.class);
	}

	if (local_node.name) free(local_node.name);
	if (local_node.app_id) free(local_node.app_id);
	if (local_node.class) free(local_node.class);
}

static void parse_json_array(const char **ptr, JsonNode *focused_node, JsonNode *current_node)
{
	if (**ptr != '[') return;
	(*ptr)++; // skip '['
	while (**ptr)
	{
		skip_space(ptr);
		if (**ptr == ']')
		{
			(*ptr)++; // skip ']'
			break;
		}
		parse_json_val(ptr, focused_node, NULL);
		skip_space(ptr);
		if (**ptr == ',')
		{
			(*ptr)++; // skip ','
		}
	}
}

static void parse_json_val(const char **ptr, JsonNode *focused_node, JsonNode *current_node)
{
	skip_space(ptr);
	if (**ptr == '{')
	{
		parse_json_object(ptr, focused_node, current_node);
	}
	else if (**ptr == '[')
	{
		parse_json_array(ptr, focused_node, current_node);
	}
	else if (**ptr == '"')
	{
		char *s = parse_json_string(ptr);
		if (s) free(s);
	}
	else
	{
		while (**ptr && **ptr != ',' && **ptr != '}' && **ptr != ']' && **ptr != ' ' && **ptr != '\n' && **ptr != '\r' && **ptr != '\t')
		{
			(*ptr)++;
		}
	}
}

static char *read_all_from_pipe(FILE *fp, size_t *out_len)
{
	size_t capacity = 4096;
	size_t len = 0;
	char *buf = malloc(capacity);
	if (!buf) return NULL;

	char chunk[4096];
	size_t n;
	while ((n = fread(chunk, 1, sizeof(chunk), fp)) > 0)
	{
		if (len + n >= capacity)
		{
			capacity *= 2;
			char *new_buf = realloc(buf, capacity);
			if (!new_buf)
			{
				free(buf);
				return NULL;
			}
			buf = new_buf;
		}
		memcpy(buf + len, chunk, n);
		len += n;
	}
	buf[len] = '\0';
	if (out_len) *out_len = len;
	return buf;
}

static void parse_hyprctl_output(FILE *fp, char **out_class, char **out_title)
{
	char line[1024];
	*out_class = NULL;
	*out_title = NULL;
	while (fgets(line, sizeof(line), fp))
	{
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;

		size_t len = strlen(p);
		while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r'))
		{
			p[len - 1] = '\0';
			len--;
		}

		if (strncmp(p, "class: ", 7) == 0)
		{
			*out_class = strdup(p + 7);
		}
		else if (strncmp(p, "title: ", 7) == 0)
		{
			*out_title = strdup(p + 7);
		}
	}
}

void free_active_window_info(ActiveWindowInfo *info)
{
	if (info)
	{
		if (info->title)
		{
			free(info->title);
		}
		if (info->class)
		{
			free(info->class);
		}
		free(info);
	}
}

ActiveWindowInfo *get_wayland_active_window_info(void)
{
	ActiveWindowInfo *ans = malloc(sizeof(ActiveWindowInfo));
	bzero(ans, sizeof(ActiveWindowInfo));
	ans->class = strdup("");
	ans->title = strdup("");

	static WaylandContext ctx;
	static int ctx_discovered = 0;
	if (!ctx_discovered)
	{
		discover_wayland_context(&ctx);
		ctx_discovered = 1;
	}

	char cmd[2048] = "";
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));

	if (ctx.is_sway)
	{
		snprintf(cmd, sizeof(cmd), "%sswaymsg -t get_tree 2>/dev/null", prefix);
	}
	else if (ctx.is_hypr)
	{
		snprintf(cmd, sizeof(cmd), "%shyprctl activewindow 2>/dev/null", prefix);
	}

	if (strlen(cmd) > 0)
	{
		FILE *fp = popen(cmd, "r");
		if (fp)
		{
			if (ctx.is_sway)
			{
				size_t len = 0;
				char *json_str = read_all_from_pipe(fp, &len);
				pclose(fp);
				if (json_str)
				{
					JsonNode focused_node;
					memset(&focused_node, 0, sizeof(focused_node));
					const char *ptr = json_str;
					parse_json_val(&ptr, &focused_node, NULL);

					if (focused_node.focused)
					{
						char *class_val = focused_node.app_id ? focused_node.app_id : focused_node.class;
						if (class_val)
						{
							free(ans->class);
							ans->class = strdup(class_val);
						}
						if (focused_node.name)
						{
							free(ans->title);
							ans->title = strdup(focused_node.name);
						}
					}

					if (focused_node.name) free(focused_node.name);
					if (focused_node.app_id) free(focused_node.app_id);
					if (focused_node.class) free(focused_node.class);
					free(json_str);
				}
			}
			else
			{
				char *class_val = NULL;
				char *title_val = NULL;
				parse_hyprctl_output(fp, &class_val, &title_val);
				pclose(fp);

				if (class_val)
				{
					free(ans->class);
					ans->class = class_val;
				}
				if (title_val)
				{
					free(ans->title);
					ans->title = title_val;
				}
			}
		}
	}
	return ans;
}
