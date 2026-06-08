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

#include "grabbing.h"
#include "grabbing-evdev.h"
#include "uinput_device.h"
#include "actions.h"
#include "action_backend.h"
#include "logging.h"

#ifndef MAX_STROKES_PER_CAPTURE
#define MAX_STROKES_PER_CAPTURE 63 /*TODO*/
#endif

const char stroke_representations[] = {' ', 'L', 'R', 'U', 'D', '1', '3', '7',
									   '9'};

static void mouse_click(Grabber *self, int button, int x, int y)
{
    execute_click_agnostic(button);
}

static void free_grabbed(Capture *free_me)
{
	assert(free_me);
	if (free_me->expression_list)
	{
		free(free_me->expression_list);
	}
	free(free_me);
}

static char get_fine_direction_from_deltas(int x_delta, int y_delta)
{

	if ((x_delta == 0) && (y_delta == 0))
	{
		return stroke_representations[NONE];
	}

	int abs_x = abs(x_delta);
	int abs_y = abs(y_delta);

	// check if the movement is near main axes
	if ((x_delta == 0) || (y_delta == 0) || (abs_x > 3 * abs_y) || (abs_y > 3 * abs_x))
	{

		// x axe
		if (abs_x > abs_y)
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

static void movement_add_direction(char *stroke_sequence, int *len_ptr, char direction)
{
	// grab stroke
	int len = *len_ptr;
	if ((len == 0) || (stroke_sequence[len - 1] != direction))
	{

		if (len < MAX_STROKES_PER_CAPTURE)
		{

			stroke_sequence[len] = direction;
			stroke_sequence[len + 1] = '\0';
			(*len_ptr)++;
		}
	}
}

/**
 * Clear previous movement data.
 */
void grabbing_start_movement(Grabber *self, int new_x, int new_y)
{

	self->started = 1;

	self->fine_direction_sequence[0] = '\0';
	self->rought_direction_sequence[0] = '\0';
	self->fine_len = 0;
	self->rought_len = 0;

	self->old_x = new_x;
	self->old_y = new_y;

	self->rought_old_x = new_x;
	self->rought_old_y = new_y;

	return;
}

void grabbing_update_movement(Grabber *self, int new_x, int new_y)
{

	if (!self->started)
	{
		return;
	}

	int x_delta = (new_x - self->old_x);
	int y_delta = (new_y - self->old_y);

	if ((abs(x_delta) > self->delta_min) || (abs(y_delta) > self->delta_min))
	{

		char stroke = get_fine_direction_from_deltas(x_delta, y_delta);
		LOG_INFO(1, "DEBUG: Stroke detected: %c (dx=%d, dy=%d)\n", stroke, x_delta, y_delta);

		movement_add_direction(self->fine_direction_sequence, &self->fine_len, stroke);

		// reset start position
		self->old_x = new_x;
		self->old_y = new_y;
	}

	int rought_delta_x = new_x - self->rought_old_x;
	int rought_delta_y = new_y - self->rought_old_y;

	int square_distance_2 = rought_delta_x * rought_delta_x + rought_delta_y * rought_delta_y;

	if (self->delta_min * self->delta_min < square_distance_2)
	{
		// grab stroke
		char rought_direction = get_direction_from_deltas(rought_delta_x,
														  rought_delta_y);

		movement_add_direction(self->rought_direction_sequence, &self->rought_len,
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

	Capture *grab = NULL;

	self->started = 0;

	// if there is no gesture
	if ((self->rought_len == 0) && (self->fine_len == 0))
	{
		if (self->is_exclusive || self->evdev)
		{
			LOG_INFO(1, "\nEmulating click\n");
			mouse_click(self, self->button, new_x, new_y);
		}
	}
	else
	{

		int expression_count = 2;
		char **expression_list = malloc(sizeof(char *) * expression_count);

		expression_list[0] = self->fine_direction_sequence;
		expression_list[1] = self->rought_direction_sequence;

		grab = malloc(sizeof(Capture));

		grab->expression_count = expression_count;
		grab->expression_list = expression_list;

		LOG_INFO(1, "DEBUG: Captured sequences: Fine='%s', Rough='%s'\n", 
				 self->fine_direction_sequence, self->rought_direction_sequence);
	}

	if (grab)
	{

		LOG_INFO(1, "\n");
		LOG_INFO(1, "     Device      : \"%s\"\n", device_name);

		Gesture *gest = configuration_process_gesture(conf, grab);

		if (gest)
		{
			LOG_INFO(1, "     Movement '%s' matched gesture '%s'\n",
				   gest->movement->name, gest->name);

			int j = 0;

			for (j = 0; j < gest->action_count; ++j)
			{
				Action *a = gest->action_list[j];
				LOG_INFO(1, "     Executing action: %s %s\n",
					   get_action_name(a->type), a->original_str);
				execute_action_agnostic(a);
			}
		}
		else
		{

			for (int i = 0; i < grab->expression_count; ++i)
			{
				char *movement = grab->expression_list[i];
				LOG_INFO(1,
					"     Sequence '%s' does not match any known movement.\n",
					movement);
			}
		}

		LOG_INFO(1, "\n");

		free_grabbed(grab);
	}
}

void grabber_set_button(Grabber *self, int button)
{
	self->button = button;
}

void grabber_set_device(Grabber *self, char *device_name)
{
	self->devicename = device_name;
	self->delta_min = 30;
}

void grabber_set_brush_color(Grabber *self, char *brush_color)
{
}

Grabber *grabber_new(char *device_name, int button)
{

	Grabber *self = malloc(sizeof(Grabber));
	bzero(self, sizeof(Grabber));

	self->fine_direction_sequence = malloc(MAX_STROKES_PER_CAPTURE + 1);
	self->rought_direction_sequence = malloc(MAX_STROKES_PER_CAPTURE + 1);
	self->fine_len = 0;
	self->rought_len = 0;

	grabber_set_device(self, device_name);
	grabber_set_button(self, button);

	return self;
}

void grabber_list_devices(Grabber *self)
{
};

void grabber_loop(Grabber *self, Configuration *conf)
{
	action_backend_init();

	if (self->evdev)
	{
		grabber_evdev_loop(self, conf);
	}

	LOG_INFO(1, "Grabbing loop finished for device '%s'.\n", self->devicename);
}

char *grabber_get_device_name(Grabber *self)
{
	return self->devicename;
}

void grabber_finalize(Grabber *self)
{
	uinput_close();
	return;
}