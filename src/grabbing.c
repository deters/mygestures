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

static void mouse_click(Grabber *self, int button, int x, int y)
{
    execute_click_agnostic(button);
}

static double perpendicular_distance(Point2D p, Point2D line_start, Point2D line_end) {
	double dx = line_end.x - line_start.x;
	double dy = line_end.y - line_start.y;
	double mag2 = dx*dx + dy*dy;
	if (mag2 < 1e-9) {
		double dx2 = p.x - line_start.x;
		double dy2 = p.y - line_start.y;
		return sqrt(dx2*dx2 + dy2*dy2);
	}
	double u = ((p.x - line_start.x) * dx + (p.y - line_start.y) * dy) / mag2;
	if (u < 0) u = 0;
	else if (u > 1) u = 1;
	double intersection_x = line_start.x + u * dx;
	double intersection_y = line_start.y + u * dy;
	double diff_x = p.x - intersection_x;
	double diff_y = p.y - intersection_y;
	return sqrt(diff_x*diff_x + diff_y*diff_y);
}

static void douglas_peucker_recursive(const Point2D *points, int start, int end, double epsilon, int *keep) {
	if (end <= start + 1) return;
	double max_dist = 0;
	int index = start;
	for (int i = start + 1; i < end; i++) {
		double dist = perpendicular_distance(points[i], points[start], points[end]);
		if (dist > max_dist) {
			max_dist = dist;
			index = i;
		}
	}
	if (max_dist > epsilon) {
		keep[index] = 1;
		douglas_peucker_recursive(points, start, index, epsilon, keep);
		douglas_peucker_recursive(points, index, end, epsilon, keep);
	}
}

Point2D *grabbing_simplify_points(const Point2D *points, int count, double epsilon, int *out_count) {
	if (count <= 2) {
		Point2D *res = malloc(sizeof(Point2D) * count);
		memcpy(res, points, sizeof(Point2D) * count);
		*out_count = count;
		return res;
	}
	int *keep = calloc(count, sizeof(int));
	keep[0] = 1;
	keep[count - 1] = 1;
	douglas_peucker_recursive(points, 0, count - 1, epsilon, keep);
	
	int keep_count = 0;
	for (int i = 0; i < count; i++) {
		if (keep[i]) keep_count++;
	}
	Point2D *simplified = malloc(sizeof(Point2D) * keep_count);
	int idx = 0;
	for (int i = 0; i < count; i++) {
		if (keep[i]) {
			simplified[idx++] = points[i];
		}
	}
	free(keep);
	*out_count = keep_count;
	return simplified;
}

void grabbing_start_movement(Grabber *self, int new_x, int new_y)
{
	self->started = 1;
	self->captured_count = 0;
	
	if (self->captured_capacity < 128) {
		self->captured_capacity = 1024;
		self->captured_points = malloc(sizeof(Point2D) * self->captured_capacity);
	}
	
	self->captured_points[0].x = new_x;
	self->captured_points[0].y = new_y;
	self->captured_count = 1;
}

void grabbing_update_movement(Grabber *self, int new_x, int new_y)
{
	if (!self->started)
	{
		return;
	}

	if (self->captured_count > 0)
	{
		double dx = new_x - self->captured_points[self->captured_count - 1].x;
		double dy = new_y - self->captured_points[self->captured_count - 1].y;
		if (dx*dx + dy*dy < 4.0) {
			return; // point hasn't moved significantly
		}
	}

	if (self->captured_count >= self->captured_capacity)
	{
		self->captured_capacity *= 2;
		self->captured_points = realloc(self->captured_points, sizeof(Point2D) * self->captured_capacity);
	}

	self->captured_points[self->captured_count].x = new_x;
	self->captured_points[self->captured_count].y = new_y;
	self->captured_count++;
}

void grabbing_end_movement(Grabber *self, int new_x, int new_y,
						   char *device_name, Configuration *conf)
{
	self->started = 0;

	// Add final coordinate
	grabbing_update_movement(self, new_x, new_y);

	double length = 0;
	for (int i = 1; i < self->captured_count; i++) {
		double dx = self->captured_points[i].x - self->captured_points[i-1].x;
		double dy = self->captured_points[i].y - self->captured_points[i-1].y;
		length += sqrt(dx*dx + dy*dy);
	}

	if (self->captured_count < 5 || length < 15.0)
	{
		if (self->is_exclusive || self->evdev)
		{
			LOG_INFO(1, "\nEmulating click\n");
			mouse_click(self, self->button, new_x, new_y);
		}
	}
	else
	{
		LOG_INFO(1, "\n");
		LOG_INFO(1, "     Device      : \"%s\"\n", device_name);
		LOG_INFO(1, "     Captured %d points, length %.2f px\n", self->captured_count, length);

		Gesture *gest = configuration_process_gesture(conf, self->captured_points, self->captured_count);

		if (gest)
		{
			LOG_INFO(1, "     Movement '%s' matched gesture '%s'\n",
				   gest->movement->name, gest->name);

			for (int j = 0; j < gest->action_count; ++j)
			{
				Action *a = gest->action_list[j];
				LOG_INFO(1, "     Executing action: %s %s\n",
					   get_action_name(a->type), a->original_str);
				execute_action_agnostic(a);
			}
		}
		else
		{
			LOG_INFO(1, "     Gesture does not match any known movement.\n");
		}

		LOG_INFO(1, "\n");
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

	self->captured_capacity = 1024;
	self->captured_points = malloc(sizeof(Point2D) * self->captured_capacity);
	self->captured_count = 0;

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