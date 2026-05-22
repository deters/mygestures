/*

 Copyright 2008-2016 Lucas Augusto Deters
 Copyright 2005 Nir Tzachar

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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>

#include <X11/extensions/XTest.h>	/* emulating device events */
#include <X11/extensions/XInput2.h> /* capturing device events */

#include "drawing/drawing-brush-image.h"

#include "grabbing.h"
#include "grabbing-synaptics.h"
#include "grabbing-evdev.h"
#include "uinput_device.h"
#include "actions.h"

#ifndef MAX_STROKES_PER_CAPTURE
#define MAX_STROKES_PER_CAPTURE 63 /*TODO*/
#endif

const char stroke_representations[] = {' ', 'L', 'R', 'U', 'D', '1', '3', '7',
									   '9'};

static void grabber_open_display(Grabber *self)
{

	self->dpy = XOpenDisplay(NULL);
	if (!self->dpy)
	{
		fprintf(stderr, "Warning: Could not open X display. Visual drawing and X11 action simulation will be disabled.\n");
		return;
	}

	if (!XQueryExtension(self->dpy, "XInputExtension", &(self->opcode),
						 &(self->event), &(self->error)))
	{
		printf("X Input extension not available.\n");
		exit(-1);
	}

	int major = 2, minor = 0;
	if (XIQueryVersion(self->dpy, &major, &minor) == BadRequest)
	{
		printf("XI2 not available. Server supports %d.%d\n", major, minor);
		exit(-1);
	}
}

static struct brush_image_t *get_brush_image(char *color)
{

	struct brush_image_t *brush_image = NULL;

	if (!color)
		brush_image = NULL;
	else if (strcasecmp(color, "red") == 0)
		brush_image = &brush_image_red;
	else if (strcasecmp(color, "green") == 0)
		brush_image = &brush_image_green;
	else if (strcasecmp(color, "yellow") == 0)
		brush_image = &brush_image_yellow;
	else if (strcasecmp(color, "white") == 0)
		brush_image = &brush_image_white;
	else if (strcasecmp(color, "purple") == 0)
		brush_image = &brush_image_purple;
	else if (strcasecmp(color, "blue") == 0)
		brush_image = &brush_image_blue;
	else
		brush_image = NULL;

	return brush_image;
}

static void grabber_init_drawing(Grabber *self)
{

	if (!self->dpy) return;

	int err = 0;
	int scr = DefaultScreen(self->dpy);

	if (self->brush_image)
	{

		err = backing_init(&(self->backing), self->dpy,
						   DefaultRootWindow(self->dpy), DisplayWidth(self->dpy, scr),
						   DisplayHeight(self->dpy, scr), DefaultDepth(self->dpy, scr));
		if (err)
		{
			fprintf(stderr, "cannot open backing store.... \n");
		}
		err = brush_init(&(self->brush), &(self->backing), self->brush_image);
		if (err)
		{
			fprintf(stderr, "cannot init brush.... \n");
		}
	}
}

static Status fetch_window_title(Display *dpy, Window w, char **out_window_title)
{
	int status;
	XTextProperty text_prop;
	char **list = NULL;
	int num = 0;

	status = XGetWMName(dpy, w, &text_prop);
	if (!status || !text_prop.value || !text_prop.nitems)
	{
		*out_window_title = strdup("");
		return 1;
	}
	status = Xutf8TextPropertyToTextList(dpy, &text_prop, &list, &num);

	if (status < Success || !num || !*list)
	{
		*out_window_title = strdup("");
	}
	else
	{
		*out_window_title = (char *)strdup(*list);
	}
	XFree(text_prop.value);
	if (list)
	{
		XFreeStringList(list);
	}

	return 1;
}

/*
 * Return a window_info struct for the focused window at a given Display.
 */
static ActiveWindowInfo *get_active_window_info(Display *dpy, Window win)
{

	ActiveWindowInfo *ans = malloc(sizeof(ActiveWindowInfo));
	bzero(ans, sizeof(ActiveWindowInfo));

	if (!dpy || win == 0)
	{
		ans->class = strdup("");
		ans->title = strdup("");
		return ans;
	}

	char *win_title = NULL;
	fetch_window_title(dpy, win, &win_title);

	char *win_class = NULL;

	XClassHint class_hints;

	int result = XGetClassHint(dpy, win, &class_hints);

	if (result)
	{

		if (class_hints.res_class != NULL)
			win_class = strdup(class_hints.res_class);

		if (class_hints.res_name != NULL)
			XFree(class_hints.res_name);

		if (class_hints.res_class != NULL)
			XFree(class_hints.res_class);
	}

	ans->class = win_class ? win_class : strdup("");
	ans->title = win_title ? win_title : strdup("");

	return ans;
}

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
		parse_json_val(ptr, focused_node, current_node);
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

static void free_active_window_info(ActiveWindowInfo *info)
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

static ActiveWindowInfo *get_wayland_active_window_info(void)
{
	ActiveWindowInfo *ans = malloc(sizeof(ActiveWindowInfo));
	bzero(ans, sizeof(ActiveWindowInfo));
	ans->class = strdup("");
	ans->title = strdup("");

	uid_t uid = 0;
	char *username = NULL;
	char *sudo_uid_env = getenv("SUDO_UID");
	char *sudo_user_env = getenv("SUDO_USER");

	if (sudo_uid_env && sudo_user_env)
	{
		uid = atoi(sudo_uid_env);
		username = strdup(sudo_user_env);
	}
	else
	{
		uid = getuid();
		if (uid == 0)
		{
			DIR *dir = opendir("/run/user");
			if (dir)
			{
				struct dirent *entry;
				while ((entry = readdir(dir)))
				{
					uid_t d_uid = atoi(entry->d_name);
					if (d_uid >= 1000)
					{
						uid = d_uid;
						struct passwd *pw = getpwuid(uid);
						if (pw)
						{
							username = strdup(pw->pw_name);
						}
						break;
					}
				}
				closedir(dir);
			}
		}
		else
		{
			struct passwd *pw = getpwuid(uid);
			if (pw)
			{
				username = strdup(pw->pw_name);
			}
		}
	}

	char sway_sock[1024] = "";
	if (uid > 0)
	{
		char path[512];
		snprintf(path, sizeof(path), "/run/user/%d", uid);
		DIR *dir = opendir(path);
		if (dir)
		{
			struct dirent *entry;
			while ((entry = readdir(dir)))
			{
				if (strncmp(entry->d_name, "sway-ipc.", 9) == 0 &&
					strstr(entry->d_name, ".sock"))
				{
					snprintf(sway_sock, sizeof(sway_sock), "/run/user/%d/%s", uid, entry->d_name);
					break;
				}
			}
			closedir(dir);
		}
	}

	char hypr_sig[256] = "";
	if (uid > 0 && strlen(sway_sock) == 0)
	{
		char path[512];
		snprintf(path, sizeof(path), "/run/user/%d/hypr", uid);
		DIR *dir = opendir(path);
		if (!dir)
		{
			snprintf(path, sizeof(path), "/tmp/hypr");
			dir = opendir(path);
		}
		if (dir)
		{
			struct dirent *entry;
			while ((entry = readdir(dir)))
			{
				if (strlen(entry->d_name) > 10)
				{
					snprintf(hypr_sig, sizeof(hypr_sig), "%s", entry->d_name);
					break;
				}
			}
			closedir(dir);
		}
	}

	char cmd[2048] = "";
	int is_sway = 0;

	if (getenv("SWAYSOCK"))
	{
		snprintf(cmd, sizeof(cmd), "swaymsg -t get_tree 2>/dev/null");
		is_sway = 1;
	}
	else if (getenv("HYPRLAND_INSTANCE_SIGNATURE"))
	{
		snprintf(cmd, sizeof(cmd), "hyprctl activewindow 2>/dev/null");
		is_sway = 0;
	}
	else if (strlen(sway_sock) > 0)
	{
		if (getuid() == 0 && username)
		{
			snprintf(cmd, sizeof(cmd),
				"sudo -u %s env SWAYSOCK=%s XDG_RUNTIME_DIR=/run/user/%d swaymsg -t get_tree 2>/dev/null",
				username, sway_sock, uid);
		}
		else
		{
			snprintf(cmd, sizeof(cmd),
				"env SWAYSOCK=%s XDG_RUNTIME_DIR=/run/user/%d swaymsg -t get_tree 2>/dev/null",
				sway_sock, uid);
		}
		is_sway = 1;
	}
	else if (strlen(hypr_sig) > 0)
	{
		if (getuid() == 0 && username)
		{
			snprintf(cmd, sizeof(cmd),
				"sudo -u %s env HYPRLAND_INSTANCE_SIGNATURE=%s XDG_RUNTIME_DIR=/run/user/%d hyprctl activewindow 2>/dev/null",
				username, hypr_sig, uid);
		}
		else
		{
			snprintf(cmd, sizeof(cmd),
				"env HYPRLAND_INSTANCE_SIGNATURE=%s XDG_RUNTIME_DIR=/run/user/%d hyprctl activewindow 2>/dev/null",
				hypr_sig, uid);
		}
		is_sway = 0;
	}

	if (strlen(cmd) > 0)
	{
		FILE *fp = popen(cmd, "r");
		if (fp)
		{
			if (is_sway)
			{
				size_t len = 0;
				char *json_str = read_all_from_pipe(fp, &len);
				pclose(fp);
				if (json_str)
				{
					JsonNode focused_node;
					memset(&focused_node, 0, sizeof(focused_node));
					const char *ptr = json_str;
					parse_json_val(&ptr, &focused_node, &focused_node);

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

	if (username) free(username);
	return ans;
}

static Window get_parent_window(Display *dpy, Window w)
{
	Window root_return, parent_return, *child_return;
	unsigned int nchildren_return;
	int ret;
	ret = XQueryTree(dpy, w, &root_return, &parent_return, &child_return,
					 &nchildren_return);

	return parent_return;
}

void grabbing_xinput_grab_start(Grabber *self)
{

	if (!self->dpy) return;

	int count = XScreenCount(self->dpy);

	int screen;
	for (screen = 0; screen < count; screen++)
	{

		Window rootwindow = RootWindow(self->dpy, screen);

		if (self->is_direct_touch)
		{

			if (!self->button)
			{
				self->button = 1;
			}

			unsigned char mask_data[2] = {
				0,
			};
			XISetMask(mask_data, XI_ButtonPress);
			XISetMask(mask_data, XI_Motion);
			XISetMask(mask_data, XI_ButtonRelease);
			XIEventMask mask = {
				XIAllDevices, sizeof(mask_data), mask_data};

			int status = XIGrabDevice(self->dpy, self->deviceid, rootwindow,
									  CurrentTime, None,
									  GrabModeAsync,
									  GrabModeAsync, False, &mask);
		}
		else
		{

			if (!self->button)
			{
				self->button = 3;
			}

			unsigned char mask_data[2] = {
				0,
			};
			XISetMask(mask_data, XI_ButtonPress);
			XISetMask(mask_data, XI_Motion);
			XISetMask(mask_data, XI_ButtonRelease);
			XIEventMask mask = {
				XIAllDevices, sizeof(mask_data), mask_data};

			int nmods = 4;
			XIGrabModifiers mods[4] = {
				{0, 0},					 // no modifiers
				{LockMask, 0},			 // Caps lock
				{Mod2Mask, 0},			 // Num lock
				{LockMask | Mod2Mask, 0} // Caps & Num lock
			};

			nmods = 1;
			mods[0].modifiers = XIAnyModifier;

			int res = XIGrabButton(self->dpy, self->deviceid, self->button,
								   rootwindow, None,
								   GrabModeAsync, GrabModeAsync, False, &mask, nmods, mods);
		}
	}
}

void grabbing_xinput_grab_stop(Grabber *self)
{

	if (!self->dpy) return;

	int count = XScreenCount(self->dpy);

	int screen;
	for (screen = 0; screen < count; screen++)
	{

		Window rootwindow = RootWindow(self->dpy, screen);

		if (self->is_direct_touch)
		{

			int status = XIUngrabDevice(self->dpy, self->deviceid, CurrentTime);
		}
		else
		{
			XIGrabModifiers modifiers[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
			XIGrabModifiers mods = {
				XIAnyModifier};
			XIUngrabButton(self->dpy, self->deviceid, self->button, rootwindow,
						   1, &mods);
		}
	}
}

static void mouse_click(Grabber *self, int button, int x, int y)
{
	if (self->evdev || self->dpy == NULL)
	{
		uinput_click(button);
	}
	else
	{
		XTestFakeMotionEvent(self->dpy, DefaultScreen(self->dpy), x, y, 0);
		XTestFakeButtonEvent(self->dpy, button, True, CurrentTime);
		XTestFakeButtonEvent(self->dpy, button, False, CurrentTime);
	}
}

static Window get_window_under_pointer(Display *dpy)
{

	Window root_return, child_return;
	int root_x_return, root_y_return;
	int win_x_return, win_y_return;
	unsigned int mask_return;
	XQueryPointer(dpy, DefaultRootWindow(dpy), &root_return, &child_return,
				  &root_x_return, &root_y_return, &win_x_return, &win_y_return,
				  &mask_return);

	Window w = child_return;
	Window parent_return;
	Window *children_return;
	unsigned int nchildren_return;
	XQueryTree(dpy, w, &root_return, &parent_return, &children_return,
			   &nchildren_return);

	return children_return[nchildren_return - 1];
}

static Window get_focused_window(Display *dpy)
{

	if (!dpy) return 0;

	Window win = 0;
	int ret, val;
	ret = XGetInputFocus(dpy, &win, &val);

	if (val == RevertToParent)
	{
		win = get_parent_window(dpy, win);
	}

	return win;
}

static void execute_action(Grabber *self, Action *action, Window focused_window)
{
	int id;
	Display *dpy = self->dpy;

	assert(action);

	if (action->type == ACTION_EXECUTE)
	{
		id = fork();
		if (id == 0)
		{
			int i = system(action->original_str);
			exit(i);
		}
		if (id < 0)
		{
			fprintf(stderr, "Error forking.\n");
		}
		return;
	}

	if (self->evdev)
	{
		dpy = NULL;
	}

	if (!dpy)
	{
		if (action->type == ACTION_KEYPRESS)
		{
			action_keypress(dpy, action->original_str);
		}
		else
		{
			execute_wayland_action(action);
		}
		return;
	}

	switch (action->type)
	{
	case ACTION_ICONIFY:
		action_iconify(dpy, focused_window);
		break;
	case ACTION_KILL:
		action_kill(dpy, focused_window);
		break;
	case ACTION_RAISE:
		action_raise(dpy, focused_window);
		break;
	case ACTION_LOWER:
		action_lower(dpy, focused_window);
		break;
	case ACTION_MAXIMIZE:
		action_maximize(dpy, focused_window);
		break;
	case ACTION_RESTORE:
		action_restore(dpy, focused_window);
		break;
	case ACTION_TOGGLE_MAXIMIZED:
		action_toggle_maximized(dpy, focused_window);
		break;
	case ACTION_KEYPRESS:
		action_keypress(dpy, action->original_str);
		break;
	default:
		fprintf(stderr, "found an unknown gesture \n");
	}

	if (dpy)
	{
		XAllowEvents(dpy, 0, CurrentTime);
	}

	return;
}

static void free_grabbed(Capture *free_me)
{
	assert(free_me);
	free_active_window_info(free_me->active_window_info);
	free(free_me);
}

static char get_fine_direction_from_deltas(int x_delta, int y_delta)
{

	if ((x_delta == 0) && (y_delta == 0))
	{
		return stroke_representations[NONE];
	}

	// check if the movement is near main axes
	if ((x_delta == 0) || (y_delta == 0) || (fabs((float)x_delta / (float)y_delta) > 3) || (fabs((float)y_delta / (float)x_delta) > 3))
	{

		// x axe
		if (abs(x_delta) > abs(y_delta))
		{

			if (x_delta > 0)
			{
				return stroke_representations[RIGHT];
			}
			else
			{
				return stroke_representations[LEFT];
			}

			// y axe
		}
		else
		{

			if (y_delta > 0)
			{
				return stroke_representations[DOWN];
			}
			else
			{
				return stroke_representations[UP];
			}
		}

		// diagonal axes
	}
	else
	{

		if (y_delta < 0)
		{
			if (x_delta < 0)
			{
				return stroke_representations[SEVEN];
			}
			else if (x_delta > 0)
			{ // RIGHT
				return stroke_representations[NINE];
			}
		}
		else if (y_delta > 0)
		{ // DOWN
			if (x_delta < 0)
			{ // RIGHT
				return stroke_representations[ONE];
			}
			else if (x_delta > 0)
			{
				return stroke_representations[THREE];
			}
		}
	}

	return stroke_representations[NONE];
}

static char get_direction_from_deltas(int x_delta, int y_delta)
{

	if (abs(y_delta) > abs(x_delta))
	{
		if (y_delta > 0)
		{
			return stroke_representations[DOWN];
		}
		else
		{
			return stroke_representations[UP];
		}
	}
	else
	{
		if (x_delta > 0)
		{
			return stroke_representations[RIGHT];
		}
		else
		{
			return stroke_representations[LEFT];
		}
	}
}

static void movement_add_direction(char *stroke_sequence, char direction)
{
	// grab stroke
	int len = strlen(stroke_sequence);
	if ((len == 0) || (stroke_sequence[len - 1] != direction))
	{

		if (len < MAX_STROKES_PER_CAPTURE)
		{

			stroke_sequence[len] = direction;
			stroke_sequence[len + 1] = '\0';
		}
	}
}

static int get_touch_status(XIDeviceInfo *device)
{

	int j = 0;

	for (j = 0; j < device->num_classes; j++)
	{
		XIAnyClassInfo *class = device->classes[j];
		XITouchClassInfo *t = (XITouchClassInfo *)class;

		if (class->type != XITouchClass)
			continue;

		if (t->mode == XIDirectTouch)
		{
			return 1;
		}
	}
	return 0;
}

static void grabber_xinput_open_devices(Grabber *self, int verbose)
{

	int ndevices;
	int i;
	XIDeviceInfo *device;
	XIDeviceInfo *devices;
	int deviceid = -1;
	devices = XIQueryDevice(self->dpy, XIAllDevices, &ndevices);
	if (verbose)
	{
		printf("\nXInput Devices:\n");
	}
	for (i = 0; i < ndevices; i++)
	{
		device = &devices[i];
		switch (device->use)
		{
		/// ṕointers
		case XIMasterPointer:
		case XISlavePointer:
		case XIFloatingSlave:
			if (strcasecmp(device->name, self->devicename) == 0)
			{
				if (verbose)
				{
					printf("   [x] '%s'\n", device->name);
				}
				self->deviceid = device->deviceid;
				self->is_direct_touch = get_touch_status(device);
			}
			else
			{
				if (verbose)
				{
					printf("   [ ] '%s'\n", device->name);
				}
			}
			break;
		case XIMasterKeyboard:
			//printf("master keyboard\n");
			break;
		case XISlaveKeyboard:
			//printf("slave keyboard\n");
			break;
		}
	}

	XIFreeDeviceInfo(devices);
}

/**
 * Clear previous movement data.
 */
void grabbing_start_movement(Grabber *self, int new_x, int new_y)
{

	self->started = 1;

	self->fine_direction_sequence[0] = '\0';
	self->rought_direction_sequence[0] = '\0';

	self->old_x = new_x;
	self->old_y = new_y;

	self->rought_old_x = new_x;
	self->rought_old_y = new_y;

	if (self->brush_image && self->dpy)
	{

		backing_save(&(self->backing), new_x - self->brush.image_width,
					 new_y - self->brush.image_height);
		brush_draw(&(self->brush), self->old_x, self->old_y);
	}
	return;
}

void grabbing_update_movement(Grabber *self, int new_x, int new_y)
{

	if (!self->started)
	{
		return;
	}

	// se for o caso, desenha o movimento na tela
	if (self->brush_image && self->dpy)
	{
		backing_save(&(self->backing), new_x - self->brush.image_width,
					 new_y - self->brush.image_height);

		brush_line_to(&(self->brush), new_x, new_y);
	}

	int x_delta = (new_x - self->old_x);
	int y_delta = (new_y - self->old_y);

	if ((abs(x_delta) > self->delta_min) || (abs(y_delta) > self->delta_min))
	{

		char stroke = get_fine_direction_from_deltas(x_delta, y_delta);

		movement_add_direction(self->fine_direction_sequence, stroke);

		// reset start position
		self->old_x = new_x;
		self->old_y = new_y;
	}

	int rought_delta_x = new_x - self->rought_old_x;
	int rought_delta_y = new_y - self->rought_old_y;

	char rought_direction = get_direction_from_deltas(rought_delta_x,
													  rought_delta_y);

	int square_distance_2 = rought_delta_x * rought_delta_x + rought_delta_y * rought_delta_y;

	if (self->delta_min * self->delta_min < square_distance_2)
	{
		// grab stroke

		movement_add_direction(self->rought_direction_sequence,
							   rought_direction);

		// reset start position
		self->rought_old_x = new_x;
		self->rought_old_y = new_y;
	}

	return;
}

/**
 *
 */
void grabbing_end_movement(Grabber *self, int new_x, int new_y,
						   char *device_name, Configuration *conf)
{

	grabbing_xinput_grab_stop(self);

	Window focused_window = get_focused_window(self->dpy);
	Window target_window = focused_window;

	Capture *grab = NULL;

	self->started = 0;

	// if is drawing
	if (self->brush_image && self->dpy)
	{
		backing_restore(&(self->backing));
	};

	// if there is no gesture
	if ((strlen(self->rought_direction_sequence) == 0) && (strlen(self->fine_direction_sequence) == 0))
	{

		if (!(self->synaptics) && (self->dpy || self->evdev))
		{

			printf("\nEmulating click\n");

			//grabbing_xinput_grab_stop(self);
			mouse_click(self, self->button, new_x, new_y);
			//grabbing_xinput_grab_start(self);
		}
	}
	else
	{

		int expression_count = 2;
		char **expression_list = malloc(sizeof(char *) * expression_count);

		expression_list[0] = self->fine_direction_sequence;
		expression_list[1] = self->rought_direction_sequence;

		ActiveWindowInfo *window_info = NULL;
		if (getenv("SWAYSOCK") || getenv("HYPRLAND_INSTANCE_SIGNATURE"))
		{
			window_info = get_wayland_active_window_info();
		}

		if ((!window_info || (strlen(window_info->class) == 0 && strlen(window_info->title) == 0)) && self->dpy)
		{
			if (window_info)
			{
				free_active_window_info(window_info);
			}
			window_info = get_active_window_info(self->dpy, target_window);
		}

		if (!window_info)
		{
			window_info = malloc(sizeof(ActiveWindowInfo));
			bzero(window_info, sizeof(ActiveWindowInfo));
			window_info->class = strdup("");
			window_info->title = strdup("");
		}

		grab = malloc(sizeof(Capture));

		grab->expression_count = expression_count;
		grab->expression_list = expression_list;
		grab->active_window_info = window_info;
	}

	if (grab)
	{

		printf("\n");
		printf("     Window title: \"%s\"\n", grab->active_window_info->title);
		printf("     Window class: \"%s\"\n", grab->active_window_info->class);
		printf("     Device      : \"%s\"\n", device_name);

		Gesture *gest = configuration_process_gesture(conf, grab);

		if (gest)
		{
			printf("     Movement '%s' matched gesture '%s' on context '%s'\n",
				   gest->movement->name, gest->name, gest->context->name);

			int j = 0;

			for (j = 0; j < gest->action_count; ++j)
			{
				Action *a = gest->action_list[j];
				printf("     Executing action: %s %s\n",
					   get_action_name(a->type), a->original_str);
				execute_action(self, a, target_window);
			}
		}
		else
		{

			for (int i = 0; i < grab->expression_count; ++i)
			{
				char *movement = grab->expression_list[i];
				printf(
					"     Sequence '%s' does not match any known movement.\n",
					movement);
			}
		}

		printf("\n");

		free_grabbed(grab);
	}

	grabbing_xinput_grab_start(self);
}

void grabber_set_button(Grabber *self, int button)
{
	self->button = button;
}

void grabber_set_device(Grabber *self, char *device_name)
{
	self->devicename = device_name;

	if (strcasecmp(self->devicename, "SYNAPTICS") == 0)
	{
		self->synaptics = 1;
		self->delta_min = 200;
	}
	else
	{
		self->synaptics = 0;
		self->delta_min = 30;
	}
}

void grabber_set_brush_color(Grabber *self, char *brush_color)
{
	self->brush_image = get_brush_image(brush_color);
}

Grabber *grabber_new(char *device_name, int button)
{

	Grabber *self = malloc(sizeof(Grabber));
	bzero(self, sizeof(Grabber));

	self->fine_direction_sequence = malloc(sizeof(char *) * 30);
	self->rought_direction_sequence = malloc(sizeof(char *) * 30);

	grabber_set_device(self, device_name);
	grabber_set_button(self, button);

	return self;
}

char *get_device_name_from_event(Grabber *self, XIDeviceEvent *data)
{
	int ndevices;
	char *device_name = NULL;
	XIDeviceInfo *device;
	XIDeviceInfo *devices;
	devices = XIQueryDevice(self->dpy, data->deviceid, &ndevices);
	if (ndevices == 1)
	{
		device = &devices[0];
		device_name = strdup(device->name);
	}

	return device_name;
}

void grabber_list_devices(Grabber *self)
{
	grabber_xinput_open_devices(self, True);
};

void grabber_xinput_loop(Grabber *self, Configuration *conf)
{

	XEvent ev;

	grabber_xinput_open_devices(self, False);
	grabbing_xinput_grab_start(self);

	while (!self->shut_down)
	{

		XNextEvent(self->dpy, &ev);

		if (ev.xcookie.type == GenericEvent && ev.xcookie.extension == self->opcode && XGetEventData(self->dpy, &ev.xcookie))
		{

			XIDeviceEvent *data = NULL;

			switch (ev.xcookie.evtype)
			{

			case XI_Motion:
				data = (XIDeviceEvent *)ev.xcookie.data;
				grabbing_update_movement(self, data->root_x, data->root_y);
				break;

			case XI_ButtonPress:
				data = (XIDeviceEvent *)ev.xcookie.data;
				grabbing_start_movement(self, data->root_x, data->root_y);
				break;

			case XI_ButtonRelease:
				data = (XIDeviceEvent *)ev.xcookie.data;

				char *device_name = get_device_name_from_event(self, data);

				grabbing_xinput_grab_stop(self);
				grabbing_end_movement(self, data->root_x, data->root_y,
									  device_name, conf);
				grabbing_xinput_grab_start(self);
				break;
			}
		}
		XFreeEventData(self->dpy, &ev.xcookie);
	}
}

void grabber_loop(Grabber *self, Configuration *conf)
{

	grabber_open_display(self);

	grabber_init_drawing(self);

	if (self->synaptics)
	{
		grabber_synaptics_loop(self, conf);
	}
	else if (self->evdev)
	{
		grabber_evdev_loop(self, conf);
	}
	else
	{
		grabber_xinput_loop(self, conf);
	}

	printf("Grabbing loop finished for device '%s'.\n", self->devicename);
}

char *grabber_get_device_name(Grabber *self)
{
	return self->devicename;
}

void grabber_finalize(Grabber *self)
{
	if (self->brush_image && self->dpy)
	{
		brush_deinit(&(self->brush));
		backing_deinit(&(self->backing));
	}

	if (self->dpy)
	{
		XCloseDisplay(self->dpy);
	}
	uinput_close();
	return;
}
