/*
 Copyright 2013-2016 Lucas Augusto Deters
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

#define _GNU_SOURCE /* needed by asprintf */
#include <stdio.h>
#include <stdlib.h>

#include <wait.h>
#include <unistd.h>

#include <sys/types.h>

#include <fcntl.h>

#include <math.h>
#include <X11/extensions/XTest.h> /* emulating device events */
#include <sys/mman.h>

#include "assert.h"
#include "string.h"
#include "config.h"

#include "gestures.h"
//#include "singleton.h"

#include "configuration.h"
#include "configuration_parser.h"
#include "drawing/drawing-brush-image.h"

#ifndef MAX_STROKES_PER_CAPTURE
#define MAX_STROKES_PER_CAPTURE 63 /*TODO*/
#endif

const char stroke_representations[] = {' ', 'L', 'R', 'U', 'D'};

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

Mygestures *mygestures_new()
{

	Mygestures *self = malloc(sizeof(Mygestures));
	bzero(self, sizeof(Mygestures));

	// self->device_name = "";
	self->root_context = NULL;

	self->rought_direction_sequence = malloc(sizeof(char) * MAX_STROKES_PER_CAPTURE);

	self->dpy = XOpenDisplay(NULL);

	return self;
}

void mygestures_load_configuration(Mygestures *self, char *filename)
{

	if (filename)
	{
		self->root_context = configuration_load_from_file(filename);
	}
	else
	{
		self->root_context = configuration_load_from_defaults();
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

void mygestures_set_brush_color(Mygestures *self, char *brush_color)
{
	self->brush_image = get_brush_image(brush_color);
}

static void grabber_init_drawing(Mygestures *self)
{

	if (self->brush_image)
	{

		assert(self->dpy);

		int err = 0;
		int scr = DefaultScreen(self->dpy);

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

/**
 * Clear previous movement data.
 */
void mygestures_start_movement(Mygestures *self, int new_x, int new_y, int delta_min)
{

	self->started = 1;

	self->rought_direction_sequence[0] = '\0';

	self->old_x = new_x;
	self->old_y = new_y;

	self->rought_old_x = new_x;
	self->rought_old_y = new_y;

	if (self->brush_image)
	{

		grabber_init_drawing(self);

		backing_save(&(self->backing), new_x - self->brush.image_width,
					 new_y - self->brush.image_height);
		brush_draw(&(self->brush), self->old_x, self->old_y);
	}
	return;
}

void mygestures_update_movement(Mygestures *self, int new_x, int new_y, int delta_min)
{

	if (!self->started)
	{
		return;
	}

	// se for o caso, desenha o movimento na tela
	if (self->brush_image)
	{
		backing_save(&(self->backing), new_x - self->brush.image_width,
					 new_y - self->brush.image_height);

		brush_line_to(&(self->brush), new_x, new_y);
	}

	int x_delta = (new_x - self->old_x);
	int y_delta = (new_y - self->old_y);

	/// se os valores já vierem na forma de deltas...
	if (self->delta_updates)
	{
		x_delta = new_x;
		y_delta = new_y;
	}

	//printf("%d, %d\n", x_delta, y_delta);

	if ((abs(x_delta) > delta_min) || (abs(y_delta) > delta_min))
	{

		if (self->delta_updates)
		{

			// reset start position
			self->old_x = self->old_x + x_delta;
			self->old_y = self->old_y + y_delta;
		}
		else
		{

			// reset start position
			self->old_x = new_x;
			self->old_y = new_y;
		}
	}

	int rought_delta_x = new_x - self->rought_old_x;
	int rought_delta_y = new_y - self->rought_old_y;

	if (self->delta_updates)
	{

		rought_delta_x = (self->rought_old_x + new_x) - self->rought_old_x;
		rought_delta_y = (self->rought_old_y + new_y) - self->rought_old_y;
	}

	char rought_direction = get_direction_from_deltas(rought_delta_x,
													  rought_delta_y);

	int square_distance_2 = rought_delta_x * rought_delta_x + rought_delta_y * rought_delta_y;

	if (delta_min * delta_min < square_distance_2)
	{
		// grab stroke

		movement_add_direction(self->rought_direction_sequence,
							   rought_direction);

		if (self->delta_updates)
		{

			// reset start position
			self->rought_old_x = self->rought_old_x + new_x;
			self->rought_old_y = self->rought_old_y + new_y;
		}
		else
		{

			// reset start position
			self->rought_old_x = new_x;
			self->rought_old_y = new_y;
		}
	}

	return;
}

static Window get_parent_window(Display *dpy, Window w)
{
	Window root_return, parent_return, *child_return;
	unsigned int nchildren_return;
	int ret;
	ret = XQueryTree(dpy, w, &root_return, &parent_return, &child_return,
					 &nchildren_return);

	if (!ret)
	{
		printf("NULL value from xquerytree on get_parent_window.");
		exit(1);
	}
	return parent_return;
}

static Window get_focused_window(Display *dpy)
{

	Window win = 0;
	int val;
	XGetInputFocus(dpy, &win, &val);

	if (val == RevertToParent)
	{
		win = get_parent_window(dpy, win);
	}

	return win;
}

static void execute_action(char *action)
{

	assert(action);

	int result = fork();

	if (result == 0)
	{
		// we are in the child process

		int i = system(action);
		exit(i);
	}
}

static void fetch_window_title(Display *dpy, Window w, char **out_window_title)
{

	XTextProperty text_prop;
	char **list;
	int num;

	int status;

	status = XGetWMName(dpy, w, &text_prop);
	if (!status || !text_prop.value || !text_prop.nitems)
	{
		*out_window_title = "";
	}
	status = Xutf8TextPropertyToTextList(dpy, &text_prop, &list, &num);

	if (status < Success || !num || !*list)
	{
		*out_window_title = "";
	}
	else
	{
		*out_window_title = (char *)strdup(*list);
	}
	XFree(text_prop.value);
	XFreeStringList(list);
}

/*
 * Return a window_info struct for the focused window at a given Display.
 */
static ActiveWindowInfo *get_active_window_info(Display *dpy, Window win)
{

	char *win_title;
	fetch_window_title(dpy, win, &win_title);

	ActiveWindowInfo *ans = malloc(sizeof(ActiveWindowInfo));
	bzero(ans, sizeof(ActiveWindowInfo));

	char *win_class = NULL;

	XClassHint class_hints;

	int result = XGetClassHint(dpy, win, &class_hints);

	if (result)
	{

		if (class_hints.res_class != NULL)
			win_class = class_hints.res_class;

		if (win_class == NULL)
		{
			win_class = "";
		}
	}

	if (win_class)
	{
		ans->class = win_class;
	}
	else
	{
		ans->class = "";
	}

	if (win_title)
	{
		ans->title = win_title;
	}
	else
	{
		ans->title = "";
	}

	return ans;
}

static void free_grabbed(Capture *free_me)
{
	assert(free_me);
	free(free_me->active_window_info);
	free(free_me->expression);
	free(free_me);
}

void mygestures_set_delta_updates(Mygestures *self, int delta_updates)
{
	self->delta_updates = 1;
}

/**
 *
 */
int mygestures_end_movement(Mygestures *self, int cancel,
							char *device_name)
{

	Capture *grab = NULL;

	self->started = 0;

	// if is drawing
	if (self->brush_image)
	{
		backing_restore(&(self->backing));
	};

	// if there is no gesture
	if ((strlen(self->rought_direction_sequence) == 0))
	{
		return 0; // TODO: turn into enum.
	}
	else
	{

		Window focused_window = get_focused_window(self->dpy);
		Window target_window = focused_window;

		ActiveWindowInfo *window_info = get_active_window_info(self->dpy,
															   target_window);

		grab = malloc(sizeof(Capture));

		grab->expression = strdup(self->rought_direction_sequence);
		grab->active_window_info = window_info;

		printf("\n");
		printf("     Window title: \"%s\"\n", grab->active_window_info->title);
		printf("     Window class: \"%s\"\n", grab->active_window_info->class);
		printf("     Device      : \"%s\"\n", device_name);

		assert(self->root_context);

		Gesture *gest = context_match_gesture(self->root_context, grab->expression, grab->active_window_info);

		if (gest)
		{
			printf("     Gesture     : '%s' detected on context '%s'\n",
				   gest->movement->name, gest->context->name);

			printf("     Command     : '%s'\n",
				   gest->action);

			execute_action(gest->action);
		}
		else
		{

			printf(
				"     Sequence '%s' does not match any known movement.\n",
				grab->expression);
		}

		printf("\n");

		free_grabbed(grab);
	}
	return 1;
}
