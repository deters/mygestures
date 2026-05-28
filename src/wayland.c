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

typedef struct WaylandContext {
	uid_t uid;
	char *username;
	char sway_sock[1024];
	char hypr_sig[256];
	int is_sway;
	int is_hypr;
} WaylandContext;

static void discover_wayland_context(WaylandContext *ctx) {
	memset(ctx, 0, sizeof(WaylandContext));

	const char *env_sway = getenv("SWAYSOCK");
	const char *env_hypr = getenv("HYPRLAND_INSTANCE_SIGNATURE");

	if (env_sway) {
		snprintf(ctx->sway_sock, sizeof(ctx->sway_sock), "%s", env_sway);
		ctx->is_sway = 1;
		return;
	}
	if (env_hypr) {
		snprintf(ctx->hypr_sig, sizeof(ctx->hypr_sig), "%s", env_hypr);
		ctx->is_hypr = 1;
		return;
	}

	char *sudo_uid_env = getenv("SUDO_UID");
	char *sudo_user_env = getenv("SUDO_USER");

	if (sudo_uid_env && sudo_user_env) {
		ctx->uid = atoi(sudo_uid_env);
		ctx->username = strdup(sudo_user_env);
	} else {
		ctx->uid = getuid();
		if (ctx->uid > 0) {
			struct passwd *pw = getpwuid(ctx->uid);
			if (pw) ctx->username = strdup(pw->pw_name);
		}
	}

	if (ctx->uid > 0) {
		char path[512];
		snprintf(path, sizeof(path), "/run/user/%d", ctx->uid);
		DIR *dir = opendir(path);
		if (dir) {
			struct dirent *entry;
			while ((entry = readdir(dir))) {
				if (strstr(entry->d_name, "sway-ipc") && strstr(entry->d_name, ".sock")) {
					snprintf(ctx->sway_sock, sizeof(ctx->sway_sock), "/run/user/%d/%s", ctx->uid, entry->d_name);
					ctx->is_sway = 1;
					break;
				}
			}
			closedir(dir);
		}

		if (!ctx->is_sway) {
			// Check for Hyprland signature in subdirectories or different patterns
			snprintf(path, sizeof(path), "/run/user/%d/hypr", ctx->uid);
			DIR *dir2 = opendir(path);
			if (dir2) {
				struct dirent *entry;
				while ((entry = readdir(dir2))) {
					if (strlen(entry->d_name) >= 40) { // Typical length of Hyprland signature
						snprintf(ctx->hypr_sig, sizeof(ctx->hypr_sig), "%s", entry->d_name);
						ctx->is_hypr = 1;
						break;
					}
				}
				closedir(dir2);
			}
		}
	}

	if (ctx->uid == 0 || (!ctx->is_sway && !ctx->is_hypr)) {
		DIR *dir = opendir("/run/user");
		if (dir) {
			struct dirent *entry;
			while ((entry = readdir(dir))) {
				uid_t d_uid = atoi(entry->d_name);
				if (d_uid >= 1000) {
					char path[512];
					snprintf(path, sizeof(path), "/run/user/%d", d_uid);
					DIR *sub_dir = opendir(path);
					if (sub_dir) {
						struct dirent *sub_entry;
						while ((sub_entry = readdir(sub_dir))) {
							if (strstr(sub_entry->d_name, "sway-ipc") && strstr(sub_entry->d_name, ".sock")) {
								snprintf(ctx->sway_sock, sizeof(ctx->sway_sock), "/run/user/%d/%s", d_uid, sub_entry->d_name);
								ctx->is_sway = 1;
								break;
							}
						}
						closedir(sub_dir);
					}

					if (!ctx->is_sway) {
						snprintf(path, sizeof(path), "/run/user/%d/hypr", d_uid);
						DIR *sub_dir2 = opendir(path);
						if (sub_dir2) {
							struct dirent *sub_entry;
							while ((sub_entry = readdir(sub_dir2))) {
								if (strlen(sub_entry->d_name) >= 40) {
									snprintf(ctx->hypr_sig, sizeof(ctx->hypr_sig), "%s", sub_entry->d_name);
									ctx->is_hypr = 1;
									break;
								}
							}
							closedir(sub_dir2);
						}
					}

					if (ctx->is_sway || ctx->is_hypr) {
						ctx->uid = d_uid;
						struct passwd *pw = getpwuid(ctx->uid);
						if (ctx->username) free(ctx->username);
						ctx->username = pw ? strdup(pw->pw_name) : NULL;
						break;
					}
				}
			}
			closedir(dir);
		}
	}
}

ActiveWindowInfo *get_wayland_active_window_info(void)
{
	ActiveWindowInfo *ans = malloc(sizeof(ActiveWindowInfo));
	bzero(ans, sizeof(ActiveWindowInfo));
	ans->class = strdup("");
	ans->title = strdup("");

	WaylandContext ctx;
	discover_wayland_context(&ctx);

	char cmd[2048] = "";

	if (ctx.is_sway)
	{
		if (getuid() == 0 && ctx.username)
		{
			snprintf(cmd, sizeof(cmd),
				"sudo -u %s env SWAYSOCK=%s XDG_RUNTIME_DIR=/run/user/%d swaymsg -t get_tree 2>/dev/null",
				ctx.username, ctx.sway_sock, ctx.uid);
		}
		else
		{
			snprintf(cmd, sizeof(cmd),
				"env SWAYSOCK=%s XDG_RUNTIME_DIR=/run/user/%d swaymsg -t get_tree 2>/dev/null",
				ctx.sway_sock, ctx.uid);
		}
	}
	else if (ctx.is_hypr)
	{
		if (getuid() == 0 && ctx.username)
		{
			snprintf(cmd, sizeof(cmd),
				"sudo -u %s env HYPRLAND_INSTANCE_SIGNATURE=%s XDG_RUNTIME_DIR=/run/user/%d hyprctl activewindow 2>/dev/null",
				ctx.username, ctx.hypr_sig, ctx.uid);
		}
		else
		{
			snprintf(cmd, sizeof(cmd),
				"env HYPRLAND_INSTANCE_SIGNATURE=%s XDG_RUNTIME_DIR=/run/user/%d hyprctl activewindow 2>/dev/null",
				ctx.hypr_sig, ctx.uid);
		}
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

	if (ctx.username) free(ctx.username);
	return ans;
}

void execute_wayland_action(Action *action) {
	const char *swaysock = getenv("SWAYSOCK");
	const char *hyprland_sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");

	if (swaysock) {
		switch (action->type) {
			case ACTION_ICONIFY:
				{
					int r = system("swaymsg move scratchpad");
					(void)r;
				}
				break;
			case ACTION_KILL:
				{
					int r = system("swaymsg kill");
					(void)r;
				}
				break;
			case ACTION_RAISE:
				{
					int r = system("swaymsg focus");
					(void)r;
				}
				break;
			case ACTION_LOWER:
				LOG_WARN("Lower action is not natively supported under Sway tiling layout.\n");
				break;
			case ACTION_MAXIMIZE:
				{
					int r = system("swaymsg fullscreen enable");
					(void)r;
				}
				break;
			case ACTION_RESTORE:
				{
					int r = system("swaymsg fullscreen disable");
					(void)r;
				}
				break;
			case ACTION_TOGGLE_MAXIMIZED:
				{
					int r = system("swaymsg fullscreen toggle");
					(void)r;
				}
				break;
			default:
				LOG_WARN("Wayland action %s is not implemented or supported under Sway.\n",
						get_action_name(action->type));
				break;
		}
		return;
	}

	if (hyprland_sig) {
		switch (action->type) {
			case ACTION_ICONIFY:
				{
					int r = system("hyprctl dispatch movetoworkspacesilent special:minimized");
					(void)r;
				}
				break;
			case ACTION_KILL:
				{
					int r = system("hyprctl dispatch killactive");
					(void)r;
				}
				break;
			case ACTION_RAISE:
				{
					int r = system("hyprctl dispatch alterzorder top");
					(void)r;
				}
				break;
			case ACTION_LOWER:
				{
					int r = system("hyprctl dispatch alterzorder bottom");
					(void)r;
				}
				break;
			case ACTION_MAXIMIZE:
				{
					int r = system("hyprctl dispatch fullscreen 1");
					(void)r;
				}
				break;
			case ACTION_RESTORE:
				{
					int r = system("hyprctl dispatch fullscreen 1"); // Toggles maximize back to normal
					(void)r;
				}
				break;
			case ACTION_TOGGLE_MAXIMIZED:
				{
					int r = system("hyprctl dispatch fullscreen 1");
					(void)r;
				}
				break;
			default:
				LOG_WARN("Wayland action %s is not implemented or supported under Hyprland.\n",
						get_action_name(action->type));
				break;
		}
		return;
	}

	// Fallback to simulating standard shortcuts via uinput virtual keyboard
	const char *desktop = getenv("XDG_CURRENT_DESKTOP");
	int is_gnome = 0;
	int is_kde = 0;
	if (desktop) {
		if (strstr(desktop, "GNOME") != NULL || strstr(desktop, "gnome") != NULL || strstr(desktop, "Ubuntu") != NULL) {
			is_gnome = 1;
		}
		if (strstr(desktop, "KDE") != NULL || strstr(desktop, "kde") != NULL) {
			is_kde = 1;
		}
	}

	if (is_gnome) {
		switch (action->type) {
			case ACTION_ICONIFY:
				action_keypress(NULL, "Super_L+h");
				break;
			case ACTION_KILL:
				action_keypress(NULL, "Alt_L+F4");
				break;
			case ACTION_RAISE:
				// Window is focused upon click/grab start, so this is generally a no-op
				break;
			case ACTION_LOWER:
				action_keypress(NULL, "Alt_L+Escape");
				break;
			case ACTION_MAXIMIZE:
				action_keypress(NULL, "Super_L+space");
				break;
			case ACTION_RESTORE:
				action_keypress(NULL, "Super_L+space");
				break;
			case ACTION_TOGGLE_MAXIMIZED:
				action_keypress(NULL, "Super_L+space");
				break;
			default:
				LOG_WARN("Wayland action %s is not implemented or supported under GNOME.\n",
						get_action_name(action->type));
				break;
		}
	} else if (is_kde) {
		switch (action->type) {
			case ACTION_ICONIFY:
				action_keypress(NULL, "Alt_L+F9");
				break;
			case ACTION_KILL:
				action_keypress(NULL, "Alt_L+F4");
				break;
			case ACTION_RAISE:
				// Window is focused upon click/grab start, so this is generally a no-op
				break;
			case ACTION_LOWER:
				action_keypress(NULL, "Alt_L+Escape");
				break;
			case ACTION_MAXIMIZE:
				action_keypress(NULL, "Super_L+Page_Up");
				break;
			case ACTION_RESTORE:
				action_keypress(NULL, "Super_L+Page_Down");
				break;
			case ACTION_TOGGLE_MAXIMIZED:
				action_keypress(NULL, "Super_L+Page_Up");
				break;
			default:
				LOG_WARN("Wayland action %s is not implemented or supported under KDE.\n",
						get_action_name(action->type));
				break;
		}
	} else {
		// General fallback
		switch (action->type) {
			case ACTION_ICONIFY:
				action_keypress(NULL, "Super_L+h");
				break;
			case ACTION_KILL:
				action_keypress(NULL, "Alt_L+F4");
				break;
			case ACTION_RAISE:
				break;
			case ACTION_LOWER:
				action_keypress(NULL, "Alt_L+Escape");
				break;
			case ACTION_MAXIMIZE:
				action_keypress(NULL, "Super_L+Up");
				break;
			case ACTION_RESTORE:
				action_keypress(NULL, "Super_L+Down");
				break;
			case ACTION_TOGGLE_MAXIMIZED:
				action_keypress(NULL, "Alt_L+F10");
				break;
			default:
				LOG_WARN("Wayland action %s is not implemented or supported.\n",
						get_action_name(action->type));
				break;
		}
	}
}
