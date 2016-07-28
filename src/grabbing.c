/*
 Copyright 2005 Nir Tzachar

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.  */
/**
 *  This class grabs mouse events and try to translate them into stroke sequences.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <X11/extensions/XInput2.h>

#include "drawing/drawing-brush-image.h"

#include "grabbing.h"
#include "gestures.h"
#include "wm.h"

#define DELTA_MIN	30 /*TODO*/
#define MAX_STROKE_SEQUENCE 63 /*TODO*/

const char *modifiers_names[] = { "SHIFT", "CTRL", "ALT", "WIN", "SCROLL", "NUM", "CAPS" };

/* valid strokes */
const char stroke_names[] = { 'N', 'L', 'R', 'U', 'D', '1', '3', '7', '9' };

static void open_display(Grabber * self) {

	self->dpy = XOpenDisplay(NULL);

	if (!XQueryExtension(self->dpy, "XInputExtension", &(self->opcode), &(self->event),
			&(self->error))) {
		printf("X Input extension not available.\n");
		exit(-1);
	}

	/* Which version of XI2? We support 2.0 */
	int major = 2, minor = 0;
	if (XIQueryVersion(self->dpy, &major, &minor) == BadRequest) {
		printf("XI2 not available. Server supports %d.%d\n", major, minor);
		exit(-1);
	}

}

static void grabbing_set_brush_color(Grabber * self, char * color) {

	assert(self);

	if (color) {

		if (strcmp(color, "red") == 0)
			self->brush_image = &brush_image_red;
		else if (strcmp(color, "green") == 0)
			self->brush_image = &brush_image_green;
		else if (strcmp(color, "yellow") == 0)
			self->brush_image = &brush_image_yellow;
		else if (strcmp(color, "white") == 0)
			self->brush_image = &brush_image_white;
		else if (strcmp(color, "purple") == 0)
			self->brush_image = &brush_image_purple;
		else if (strcmp(color, "blue") == 0)
			self->brush_image = &brush_image_blue;
		else
			printf("no such color, %s. using \"blue\"\n", color);

	}
}

/*
 * Get the title of a given window at out_window_title.
 *
 * PRIVATE
 */
static Status fetch_window_title(Display *dpy, Window w, char **out_window_title) {
	int status;
	XTextProperty text_prop;
	char **list;
	int num;

	status = XGetWMName(dpy, w, &text_prop);
	if (!status || !text_prop.value || !text_prop.nitems) {
		*out_window_title = "";
	}
	status = Xutf8TextPropertyToTextList(dpy, &text_prop, &list, &num);

	if (status < Success || !num || !*list) {
		*out_window_title = "";
	} else {
		*out_window_title = (char *) strdup(*list);
	}
	XFree(text_prop.value);
	XFreeStringList(list);

	return 1;
}

/*
 * Return a window_info struct for the focused window at a given Display.
 *
 * PRIVATE
 */
static Window_info * get_window_info(Display* dpy, Window win) {

	int ret, val;

	char *win_title;
	ret = fetch_window_title(dpy, win, &win_title);

	Window_info *ans = malloc(sizeof(Window_info));
	bzero(ans, sizeof(Window_info));

	char *win_class = NULL;

	XClassHint class_hints;

	int result = XGetClassHint(dpy, win, &class_hints);

	if (result) {

		if (class_hints.res_class != NULL)
			win_class = class_hints.res_class;

		if (win_class == NULL) {
			win_class = "";

		}
	}

	if (win_class) {
		ans->class = win_class;
	} else {
		ans->class = "";
	}

	if (win_title) {
		ans->title = win_title;
	} else {
		ans->title = "";
	}

	return ans;

}

/*
 * Get the parent window.
 *
 * PRIVATE
 */
static Window get_parent_window(Display *dpy, Window w) {
	Window root_return, parent_return, *child_return;
	unsigned int nchildren_return;
	int ret;
	ret = XQueryTree(dpy, w, &root_return, &parent_return, &child_return, &nchildren_return);

	return parent_return;
}

void grabbing_grab(Grabber * self) {

	int count = XScreenCount(self->dpy);

	XIEventMask * eventmask = malloc(sizeof(XIEventMask));
	eventmask->deviceid = self->deviceid;

	int screen;
	for (screen = 0; screen < count; screen++) {

		Window rootwindow = RootWindow(self->dpy, screen);

		if (self->is_direct_touch) {

			if (!self->button) {
				self->button = 1;
			}

			unsigned char mask[1] = { 0 }; /* the actual mask */

			eventmask->mask_len = sizeof(mask); /* always in bytes */
			eventmask->mask = mask;

			XISetMask(mask, XI_ButtonPress);
			XISetMask(mask, XI_ButtonRelease);
			XISetMask(mask, XI_Motion);

			int status = XIGrabDevice(self->dpy, self->deviceid, rootwindow,
			CurrentTime, None,
			GrabModeAsync,
			GrabModeAsync, False, eventmask);

		} else {

			if (!self->button) {
				self->button = 3;
			}

			XIGrabModifiers modifiers[4] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };

			static unsigned char mask[2];
			eventmask->mask = mask;

			eventmask->mask_len = sizeof(mask);

			memset(eventmask->mask, 0, eventmask->mask_len);
			XISetMask(eventmask->mask, XI_ButtonPress);
			XISetMask(eventmask->mask, XI_ButtonRelease);
			XISetMask(eventmask->mask, XI_Motion);

			XIGrabButton(self->dpy, self->deviceid, self->button, rootwindow,
			None, GrabModeAsync, GrabModeAsync, False, eventmask, 1, modifiers);

		}

	}

}

void grabbing_ungrab(Grabber * self) {

	int count = XScreenCount(self->dpy);

	int screen;
	for (screen = 0; screen < count; screen++) {

		Window rootwindow = RootWindow(self->dpy, screen);

		/* select on the window */
		//XISelectEvents(display, w, &eventmask, 1);
		if (self->is_direct_touch) {

			int status = XIUngrabDevice(self->dpy, self->deviceid, CurrentTime);
			printf("ungrab status = %d \n", status);

		} else {
			XIGrabModifiers modifiers[4] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };

			XIUngrabButton(self->dpy, self->deviceid, self->button, rootwindow, 1, modifiers);
		}

	}

}

/*
 * Emulate a mouse click at the given display.
 *
 * PRIVATE
 */
static void mouse_click(Display *display, int button, int x, int y) {

	// fix position
	XTestFakeMotionEvent(display, DefaultScreen(display), x, y, 0);

	XTestFakeButtonEvent(display, button, True, CurrentTime);
	XTestFakeButtonEvent(display, button, False, CurrentTime);
}

/**
 * Clear previous movement data.
 */
void grabbing_start_movement(Grabber * self, int new_x, int new_y) {

	self->started = 1;

	// clear captured sequences
	self->fine_direction_sequence[0] = '\0';
	self->rought_direction_sequence[0] = '\0';

	// guarda a localização do início do movimento
	self->old_x = new_x;
	self->old_y = new_y;

	self->rought_old_x = new_x;
	self->rought_old_y = new_y;

	if (!self->without_brush) {

		XFlush(self->dpy);
		backing_save(&(self->backing), new_x - self->brush.image_width,
				new_y - self->brush.image_height);
		//backing_save(&(self->backing), new_x - self->brush.image_width,
		//		new_y - self->brush.image_height);

		brush_draw(&(self->brush), self->old_x, self->old_y);

	}
	return;
}

/*
 * Return the focused window at the given display.
 *
 * PRIVATE
 */
static Window get_focused_window(Display *dpy) {

	Window win = 0;
	int ret, val;
	ret = XGetInputFocus(dpy, &win, &val);

	if (val == RevertToParent) {
		win = get_parent_window(dpy, win);
	}

	return win;

}

/* release a key_press struct */
static void free_key_press(struct key_press *free_me) {
	free(free_me);
	return;
}

/**
 * Execute an action
 */
static void execute_action(Display *dpy, Action *action, Window focused_window) {
	int id;

	// if there is an action
	if (action != NULL) {

		switch (action->type) {
		case ACTION_EXECUTE:
			id = fork();
			if (id == 0) {
				int i = system(action->original_str);
				exit(i);
			}
			if (id < 0) {
				fprintf(stderr, "Error forking.\n");
			}

			break;
		case ACTION_ICONIFY:
			generic_iconify(dpy, focused_window);
			break;
		case ACTION_KILL:
			generic_kill(dpy, focused_window);
			break;
		case ACTION_RAISE:
			generic_raise(dpy, focused_window);
			break;
		case ACTION_LOWER:
			generic_lower(dpy, focused_window);
			break;
		case ACTION_MAXIMIZE:
			generic_maximize(dpy, focused_window);
			break;
		case ACTION_ROOT_SEND:
			generic_root_send(dpy, action->original_str);
			break;
		default:
			fprintf(stderr, "found an unknown gesture \n");
		}
	}
	return;
}

/**
 * Obtém o resultado dos dois algoritmos de captura de movimentos, e envia para serem processadas.
 */
Grabbed * grabbing_end_movement(Grabber * self, int new_x, int new_y) {

	Grabbed * result = NULL;

	self->started = 0;

	// if is drawing
	if (!self->without_brush) {
		backing_restore(&(self->backing));
		XFlush(self->dpy);
	};

	if ((strlen(self->rought_direction_sequence) == 0)
			&& (strlen(self->fine_direction_sequence) == 0)) {

		// temporary ungrab button
		grabbing_ungrab(self);

		// emulate the click
		mouse_click(self->dpy, self->button, new_x, new_y);

		// restart grabbing
		grabbing_grab(self);

	} else {

		int sequences_count = 2;
		char ** sequences = malloc(sizeof(char *) * sequences_count);

		sequences[0] = self->fine_direction_sequence;
		sequences[1] = self->rought_direction_sequence;

		Window_info * focused_window = get_window_info(self->dpy, get_focused_window(self->dpy));

		result = malloc(sizeof(Grabbed));

		result->sequences_count = sequences_count;
		result->sequences = sequences;
		result->focused_window = focused_window;

	}

	return result;
}

static char get_fine_direction_from_deltas(int x_delta, int y_delta) {

	if ((x_delta == 0) && (y_delta == 0)) {
		return stroke_names[NONE];
	}

	// check if the movement is near main axes
	if ((x_delta == 0) || (y_delta == 0) || (fabs((float) x_delta / (float) y_delta) > 3)
			|| (fabs((float) y_delta / (float) x_delta) > 3)) {

		// x axe
		if (abs(x_delta) > abs(y_delta)) {

			if (x_delta > 0) {
				return stroke_names[RIGHT];
			} else {
				return stroke_names[LEFT];
			}

			// y axe
		} else {

			if (y_delta > 0) {
				return stroke_names[DOWN];
			} else {
				return stroke_names[UP];
			}

		}

		// diagonal axes
	} else {

		if (y_delta < 0) {
			if (x_delta < 0) {
				return stroke_names[SEVEN];
			} else if (x_delta > 0) { // RIGHT
				return stroke_names[NINE];
			}
		} else if (y_delta > 0) { // DOWN
			if (x_delta < 0) { // RIGHT
				return stroke_names[ONE];
			} else if (x_delta > 0) {
				return stroke_names[THREE];
			}
		}

	}

	return stroke_names[NONE];

}

static char get_direction_from_deltas(int x_delta, int y_delta) {

	if (abs(y_delta) > abs(x_delta)) {
		if (y_delta > 0) {
			return stroke_names[DOWN];
		} else {
			return stroke_names[UP];
		}

	} else {
		if (x_delta > 0) {
			return stroke_names[RIGHT];
		} else {
			return stroke_names[LEFT];
		}

	}

}

static void movement_add_direction(char* stroke_sequence, char direction) {
	// grab stroke
	int len = strlen(stroke_sequence);
	if ((len == 0) || (stroke_sequence[len - 1] != direction)) {

		if (len < MAX_STROKE_SEQUENCE) {

			stroke_sequence[len] = direction;
			stroke_sequence[len + 1] = '\0';

		}

	}
}

static void update_movement(Grabber * self, int new_x, int new_y) {

	if (!self->started) {
		return;
	}

	// se for o caso, desenha o movimento na tela
	if (!self->without_brush) {
		backing_save(&(self->backing), new_x - self->brush.image_width,
				new_y - self->brush.image_height);
		//backing_save(&(self->backing), new_x - self->brush.image_width,
		//		new_y - self->brush.image_height);

		brush_line_to(&(self->brush), new_x, new_y);
		XFlush(self->dpy);
	}

	int x_delta = (new_x - self->old_x);
	int y_delta = (new_y - self->old_y);

	if ((abs(x_delta) > DELTA_MIN) || (abs(y_delta) > DELTA_MIN)) {

		char stroke = get_fine_direction_from_deltas(x_delta, y_delta);

		movement_add_direction(self->fine_direction_sequence, stroke);

		// reset start position
		self->old_x = new_x;
		self->old_y = new_y;

	}

	int rought_delta_x = new_x - self->rought_old_x;
	int rought_delta_y = new_y - self->rought_old_y;

	char rought_direction = get_direction_from_deltas(rought_delta_x, rought_delta_y);

	int square_distance_2 = rought_delta_x * rought_delta_x + rought_delta_y * rought_delta_y;

	if ( DELTA_MIN * DELTA_MIN < square_distance_2) {
		// grab stroke

		movement_add_direction(self->rought_direction_sequence, rought_direction);

		// reset start position
		self->rought_old_x = new_x;
		self->rought_old_y = new_y;
	}

	return;
}

static void free_grabbed(Grabbed * free_me) {
	assert(free_me);
	free(free_me->focused_window);
	free(free_me);
}

static int get_touch_status(XIDeviceInfo * device) {

	int j = 0;

	for (j = 0; j < device->num_classes; j++) {
		XIAnyClassInfo *class = device->classes[j];
		XITouchClassInfo *t = (XITouchClassInfo*) class;

		if (class->type != XITouchClass)
			continue;

		if (t->mode == XIDirectTouch) {
			return 1;
		}
		/*
		 if (print_devices) {
		 printf("        %s touch device, supporting %d touches. ",
		 (t->mode == XIDirectTouch) ? "direct" : "dependent",
		 t->num_touches);
		 }
		 */
	}
	return 0;
}

Grabber * grabber_init(char * device_name, int button, int without_brush, int print_devices, char * brush_color) {

	Grabber * self = malloc(sizeof(Grabber));
	bzero(self, sizeof(Grabber));

	assert(self);

	open_display(self);

	self->button = button;
	self->without_brush = without_brush;

	self->devicename = device_name;
	if (!self->devicename) {
		self->devicename = "Virtual core pointer";
	}

	grabbing_set_brush_color(self, brush_color);

	int ndevices;
	int i;
	XIDeviceInfo * device;
	XIDeviceInfo * devices;

	int deviceid = -1;

	devices = XIQueryDevice(self->dpy, XIAllDevices, &ndevices);

	if (print_devices) {
		printf("\nDevices:\n");
	}

	for (i = 0; i < ndevices; i++) {
		device = &devices[i];

		switch (device->use) {
		/// ṕointers
		case XIMasterPointer:
		case XISlavePointer:
		case XIFloatingSlave:

			if (print_devices) {
				printf("   '%s'\n", device->name);
			} else {

				if (strcmp(device->name, self->devicename) == 0) {
					self->deviceid = device->deviceid;
					self->is_direct_touch = get_touch_status(device);
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

	if (print_devices) {
		printf("\nRun  mygestures -d 'DEVICE'  to use a device.\n");
	}

	XIFreeDeviceInfo(devices);

	if (print_devices) {
		exit(0);
	}

	self->fine_direction_sequence = (char *) malloc(sizeof(char) * (MAX_STROKE_SEQUENCE + 1));
	self->fine_direction_sequence[0] = '\0';

	self->rought_direction_sequence = (char *) malloc(sizeof(char) * (MAX_STROKE_SEQUENCE + 1));
	self->rought_direction_sequence[0] = '\0';

	int err = 0;
	int scr = DefaultScreen(self->dpy);

	XAllowEvents(self->dpy, AsyncBoth, CurrentTime);

	if (!self->without_brush) {

		if (!self->brush_image) {
			self->brush_image = &brush_image_red;
		}

		err = backing_init(&(self->backing), self->dpy, DefaultRootWindow(self->dpy),
				DisplayWidth(self->dpy, scr), DisplayHeight(self->dpy, scr),
				DefaultDepth(self->dpy, scr));
		if (err) {
			fprintf(stderr, "cannot open backing store.... \n");
			return NULL;
		}

		err = brush_init(&(self->brush), &(self->backing), self->brush_image);
		if (err) {
			fprintf(stderr, "cannot init brush.... \n");
			return NULL;
		}
	}

	self->old_x = -1;
	self->old_y = -1;

	self->rought_old_x = -1;
	self->rought_old_y = -1;

	return self;
}

void grabber_event_loop(Grabber * self, Engine * conf) {

	XEvent ev;

	grabbing_grab(self);

	printf("\n");
	if (self->is_direct_touch) {
		printf(
				"Mygestures is running on device '%s'. Draw a gesture by touching the screen or run `mygestures -l` to list other devices.\n",
				self->devicename);
	} else {
		printf(
				"Mygestures is running on device '%s'. Use button %d on this device to draw a gesture or run `mygestures -l` to list other devices.\n",
				self->devicename, self->button);
	}

	printf("\n");

	while (!self->shut_down) {

		XNextEvent(self->dpy, &ev);

		if (ev.xcookie.type == GenericEvent && ev.xcookie.extension == self->opcode
				&& XGetEventData(self->dpy, &ev.xcookie)) {

			XIDeviceEvent* data = NULL;

			switch (ev.xcookie.evtype) {

			case XI_Motion:
				data = (XIDeviceEvent*) ev.xcookie.data;
				update_movement(self, data->root_x, data->root_y);
				break;

			case XI_ButtonPress:
				data = (XIDeviceEvent*) ev.xcookie.data;
				grabbing_start_movement(self, data->root_x, data->root_y);
				break;

			case XI_ButtonRelease:
				data = (XIDeviceEvent*) ev.xcookie.data;
				Grabbed * grab = grabbing_end_movement(self, data->root_x, data->root_y);

				if (grab) {

					printf("\n");
					printf("Window information:\n");
					printf("     Title = \"%s\"\n", grab->focused_window->title);
					printf("     Class = \"%s\"\n", grab->focused_window->class);

					Gesture * gest = engine_process_gesture(conf, grab);

					if (gest){
						printf("Gesture information:\n");
						printf("     Movement '%s' triggered gesture '%s'\n",
							gest->movement->name, gest->name);
					}


					if (gest) {

						int j = 0;

						for (j = 0; j < gest->actions_count; ++j) {
							Action * a = gest->actions[j];
							printf("      Action: %s\n", a->original_str);
							execute_action(self->dpy, a, get_focused_window(self->dpy));
						}

					}

					free_grabbed(grab);

				}

				break;

			}
		}
		XFreeEventData(self->dpy, &ev.xcookie);

	}

	grabbing_ungrab(self);

}

int grabber_get_device_id(Grabber * self) {
	return self->deviceid;
}


char * grabber_get_device_name(Grabber * self) {
	return self->devicename;
}

void grabber_finalize(Grabber * self) {
	if (!(self->without_brush)) {
		brush_deinit(&(self->brush));
		backing_deinit(&(self->backing));
	}

	XCloseDisplay(self->dpy);
	return;
}

