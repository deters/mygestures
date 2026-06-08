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

#if HAVE_CONFIG_H          
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "configuration.h"
#include "actions.h"

static double path_length(const Point2D *points, int count) {
	double d = 0;
	for (int i = 1; i < count; i++) {
		double dx = points[i].x - points[i-1].x;
		double dy = points[i].y - points[i-1].y;
		d += sqrt(dx*dx + dy*dy);
	}
	return d;
}

static Point2D *resample_path(const Point2D *points, int count, int n) {
	Point2D *resampled = malloc(sizeof(Point2D) * n);
	if (!resampled) return NULL;
	if (count == 0) {
		memset(resampled, 0, sizeof(Point2D) * n);
		return resampled;
	}
	if (count == 1) {
		for (int i = 0; i < n; i++) resampled[i] = points[0];
		return resampled;
	}
	
	double len = path_length(points, count);
	double I = (len > 1e-4) ? (len / (n - 1)) : 0.0;
	double D = 0;
	resampled[0] = points[0];
	int r_count = 1;
	
	Point2D *pts = malloc(sizeof(Point2D) * count);
	memcpy(pts, points, sizeof(Point2D) * count);
	
	for (int i = 1; i < count; i++) {
		double dx = pts[i].x - pts[i-1].x;
		double dy = pts[i].y - pts[i-1].y;
		double d = sqrt(dx*dx + dy*dy);
		if ((D + d) >= I && I > 1e-4) {
			double qx = pts[i-1].x + ((I - D) / d) * dx;
			double qy = pts[i-1].y + ((I - D) / d) * dy;
			resampled[r_count++] = (Point2D){qx, qy};
			pts[i-1] = (Point2D){qx, qy};
			i--; // check the same segment again
			D = 0;
		} else {
			D += d;
		}
	}
	free(pts);
	
	while (r_count < n) {
		resampled[r_count++] = points[count - 1];
	}
	return resampled;
}

static void normalize_path(Point2D *points, int count, double size) {
	if (count == 0) return;
	double min_x = points[0].x, max_x = points[0].x;
	double min_y = points[0].y, max_y = points[0].y;
	for (int i = 1; i < count; i++) {
		if (points[i].x < min_x) min_x = points[i].x;
		if (points[i].x > max_x) max_x = points[i].x;
		if (points[i].y < min_y) min_y = points[i].y;
		if (points[i].y > max_y) max_y = points[i].y;
	}
	double width = max_x - min_x;
	double height = max_y - min_y;
	double max_dim = (width > height) ? width : height;
	if (max_dim < 1e-4) max_dim = 1e-4;
	
	double scale = size / max_dim;
	double sum_x = 0, sum_y = 0;
	for (int i = 0; i < count; i++) {
		points[i].x *= scale;
		points[i].y *= scale;
		sum_x += points[i].x;
		sum_y += points[i].y;
	}
	double centroid_x = sum_x / count;
	double centroid_y = sum_y / count;
	for (int i = 0; i < count; i++) {
		points[i].x -= centroid_x;
		points[i].y -= centroid_y;
	}
}

static double path_distance(const Point2D *p1, const Point2D *p2, int n) {
	double dist = 0;
	for (int i = 0; i < n; i++) {
		double dx = p1[i].x - p2[i].x;
		double dy = p1[i].y - p2[i].y;
		dist += sqrt(dx*dx + dy*dy);
	}
	return dist / n;
}

void movement_set_expression(Movement* movement, char* movement_expression) {
	movement->expression = movement_expression;
	if (movement->points) {
		free(movement->points);
		movement->points = NULL;
	}
	movement->point_count = 0;
	
	int count = 0;
	char *expr_copy = strdup(movement_expression);
	char *token = strtok(expr_copy, " ");
	while (token) {
		count++;
		token = strtok(NULL, " ");
	}
	free(expr_copy);
	
	if (count == 0) {
		movement->points = NULL;
		return;
	}
	
	movement->points = malloc(sizeof(Point2D) * count);
	movement->point_count = count;
	
	expr_copy = strdup(movement_expression);
	token = strtok(expr_copy, " ");
	int idx = 0;
	while (token) {
		double x = 0, y = 0;
		if (sscanf(token, "%lf,%lf", &x, &y) == 2) {
			movement->points[idx].x = x;
			movement->points[idx].y = y;
			idx++;
		}
		token = strtok(NULL, " ");
	}
	free(expr_copy);
	movement->point_count = idx;
}

/* alloc a movement struct */
Movement *configuration_create_movement(Configuration * self,
		char *movement_name, char *movement_expression) {

	assert(self);
	assert(movement_name);
	assert(movement_expression);

	Movement * movement = malloc(sizeof(Movement));
	bzero(movement, sizeof(Movement));

	movement->name = movement_name;
	movement_set_expression(movement, movement_expression);

	if (self->movement_count < 254) {
		self->movement_list[self->movement_count++] = movement;
	} else {
		fprintf(stderr, "Warning: Maximum movements (254) reached. Ignoring movement '%s'.\n", movement_name);
	}

	return movement;
}

Gesture * configuration_create_gesture(Configuration * self, char * gesture_name,
		char * gesture_movement_or_stroke) {

	assert(self);
	assert(gesture_name);
	assert(gesture_movement_or_stroke);

	Gesture *ans = malloc(sizeof(Gesture));
	bzero(ans, sizeof(Gesture));

	ans->name = strdup(gesture_name);
	ans->movement = configuration_find_movement_by_name(
			self, gesture_movement_or_stroke);

	if (!ans->movement) {
		// Treat as a raw stroke and create an anonymous movement
		ans->movement = malloc(sizeof(Movement));
		bzero(ans->movement, sizeof(Movement));
		ans->movement->name = strdup("anonymous");
		movement_set_expression(ans->movement, strdup(gesture_movement_or_stroke));
	}

	ans->action_count = 0;
	ans->action_list = malloc(sizeof(Action *) * 20);

	if (self->gesture_count < 255) {
		self->gesture_list[self->gesture_count++] = ans;
	} else {
		fprintf(stderr, "Warning: Maximum gestures (255) reached. Ignoring gesture '%s'.\n", gesture_name);
	}

	return ans;
}

void configuration_add_action_from_string(Gesture * self, const char * action_str) {
	assert(self);
	assert(action_str);

	char *copy = strdup(action_str);
	char *action_name = copy;
	char *value = strchr(copy, ' ');
	
	if (value) {
		*value = '\0';
		value++;
		while (*value == ' ') value++; // Skip extra spaces
	} else {
		value = "";
	}

	int id = ACTION_NULL;

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
	} else if (strcasecmp(action_name, "restore") == 0) {
		id = ACTION_RESTORE;
	} else if (strcasecmp(action_name, "toggle-maximized") == 0) {
		id = ACTION_TOGGLE_MAXIMIZED;
	} else if (strcasecmp(action_name, "keypress") == 0 || strcasecmp(action_name, "keys") == 0) {
		id = ACTION_KEYPRESS;
	} else if (strcasecmp(action_name, "exec") == 0) {
		id = ACTION_EXECUTE;
	} else if (strcasecmp(action_name, "workspace-left") == 0) {
		id = ACTION_WORKSPACE_LEFT;
	} else if (strcasecmp(action_name, "workspace-right") == 0) {
		id = ACTION_WORKSPACE_RIGHT;
	} else if (strcasecmp(action_name, "workspace-up") == 0) {
		id = ACTION_WORKSPACE_UP;
	} else if (strcasecmp(action_name, "workspace-down") == 0) {
		id = ACTION_WORKSPACE_DOWN;
	} else if (strcasecmp(action_name, "show-overview") == 0) {
		id = ACTION_SHOW_OVERVIEW;
	} else if (strcasecmp(action_name, "show-app-grid") == 0) {
		id = ACTION_SHOW_APP_GRID;
	} else if (strcasecmp(action_name, "click") == 0) {
		id = ACTION_CLICK;
	} else if (strcasecmp(action_name, "toggle-fullscreen") == 0) {
		id = ACTION_TOGGLE_FULLSCREEN;
	} else if (strcasecmp(action_name, "show-desktop") == 0) {
		id = ACTION_SHOW_DESKTOP;
	} else if (strcasecmp(action_name, "lock-screen") == 0) {
		id = ACTION_LOCK_SCREEN;
	} else if (strcasecmp(action_name, "terminal") == 0) {
		id = ACTION_TERMINAL;
	} else if (strcasecmp(action_name, "volume-up") == 0) {
		id = ACTION_VOLUME_UP;
	} else if (strcasecmp(action_name, "volume-down") == 0) {
		id = ACTION_VOLUME_DOWN;
	} else if (strcasecmp(action_name, "volume-mute") == 0) {
		id = ACTION_VOLUME_MUTE;
	} else if (strcasecmp(action_name, "media-play") == 0) {
		id = ACTION_MEDIA_PLAY;
	} else if (strcasecmp(action_name, "media-next") == 0) {
		id = ACTION_MEDIA_NEXT;
	} else if (strcasecmp(action_name, "media-prev") == 0) {
		id = ACTION_MEDIA_PREV;
	} else if (strcasecmp(action_name, "www") == 0) {
		id = ACTION_WWW;
	} else if (strcasecmp(action_name, "home") == 0) {
		id = ACTION_HOME;
	} else if (strcasecmp(action_name, "email") == 0) {
		id = ACTION_EMAIL;
	} else if (strcasecmp(action_name, "search") == 0) {
		id = ACTION_SEARCH;
	} else if (strcasecmp(action_name, "calculator") == 0) {
		id = ACTION_CALCULATOR;
	} else if (strcasecmp(action_name, "control-center") == 0) {
		id = ACTION_CONTROL_CENTER;
	} else if (strcasecmp(action_name, "logout") == 0) {
		id = ACTION_LOGOUT;
	} else if (strcasecmp(action_name, "screenshot") == 0) {
		id = ACTION_SCREENSHOT;
	} else if (strcasecmp(action_name, "screenshot-window") == 0) {
		id = ACTION_SCREENSHOT_WINDOW;
	} else if (strcasecmp(action_name, "screenshot-area") == 0) {
		id = ACTION_SCREENSHOT_AREA;
	} else {
		fprintf(stderr, "Warning: unknown action '%s' in gesture '%s'\n", action_name, self->name);
		free(copy);
		return;
	}

	configuration_create_action(self, id, strdup(value));
	free(copy);
}

/* alloc an action struct */
Action *configuration_create_action(Gesture * self, int action_type,
		char * action_data) {

	assert(self);
	assert(action_type);
	assert(action_data);

	Action *ans = malloc(sizeof(Action));
	bzero(ans, sizeof(Action));

	ans->type = action_type;
	ans->original_str = action_data;

	if (self->action_count < 20) {
		self->action_list[self->action_count++] = ans;
	} else {
		fprintf(stderr, "Warning: Maximum actions (20) reached for gesture '%s'. Ignoring extra actions.\n", self->name);
	}

	return ans;
}

Gesture * match_gesture(Configuration * self, const Point2D * captured_points, int point_count) {
	assert(self);
	if (!captured_points || point_count < 2) return NULL;
	
	int N = 64;
	double BOX_SIZE = 250.0;
	double THRESHOLD = 55.0; // Distance threshold for gesture matching
	
	// Normalize the input path
	Point2D *input_resampled = resample_path(captured_points, point_count, N);
	if (!input_resampled) return NULL;
	normalize_path(input_resampled, N, BOX_SIZE);
	
	Gesture *best_gesture = NULL;
	double min_dist = THRESHOLD;
	
	for (int g = 0; g < self->gesture_count; g++) {
		Gesture *gest = self->gesture_list[g];
		if (!gest || !gest->movement || gest->movement->point_count < 2) continue;
		
		// Normalize the template path
		Point2D *temp_resampled = resample_path(gest->movement->points, gest->movement->point_count, N);
		if (!temp_resampled) continue;
		normalize_path(temp_resampled, N, BOX_SIZE);
		
		double dist = path_distance(input_resampled, temp_resampled, N);
		free(temp_resampled);
		
		if (dist < min_dist) {
			min_dist = dist;
			best_gesture = gest;
		}
	}
	
	free(input_resampled);
	return best_gesture;
}

Gesture * configuration_process_gesture(Configuration * self, const Point2D * points, int point_count) {
	assert(self);
	if (!points || point_count < 2) return NULL;
	return match_gesture(self, points, point_count);
}

Movement * configuration_find_movement_by_name(Configuration * self,
		char * movement_name) {

	assert(self);
	assert(movement_name);

	if (!movement_name) {
		return NULL;
	}

	int i = 0;

	for (i = 0; i < self->movement_count; ++i) {
		Movement * m = self->movement_list[i];

		if ((m->name) && (movement_name)
				&& (strcasecmp(movement_name, m->name) == 0)) {
			return m;
		}
	}

	return NULL;

}

int configuration_get_gestures_count(Configuration * self) {

	assert(self);

	return self->gesture_count;

}

Configuration * configuration_new() {

	Configuration * self = malloc(sizeof(Configuration));
	bzero(self, sizeof(Configuration));

	self->movement_count = 0;
	self->movement_list = malloc(sizeof(Movement *) * 254);

	self->gesture_count = 0;
	self->gesture_list = malloc(sizeof(Gesture *) * 254);

	return self;

}
