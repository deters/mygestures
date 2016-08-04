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


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <X11/extensions/XTest.h> /* emulating device events */
#include <X11/extensions/XInput2.h> /* capturing device events */
#include <sys/shm.h> /* needed for synaptics */
#include <sys/time.h>

#include "drawing/drawing-brush-image.h"

#include "grabbing.h"
#include "actions.h"

#ifndef MAX_STROKES_PER_CAPTURE
#define MAX_STROKES_PER_CAPTURE 63 /*TODO*/
#endif

const char stroke_representations[] = { ' ', 'L', 'R', 'U', 'D', '1', '3', '7', '9' };

static void grabber_open_display(Grabber * self) {

	self->dpy = XOpenDisplay(NULL);

	if (!XQueryExtension(self->dpy, "XInputExtension", &(self->opcode), &(self->event),
			&(self->error))) {
		printf("X Input extension not available.\n");
		exit(-1);
	}

	int major = 2, minor = 0;
	if (XIQueryVersion(self->dpy, &major, &minor) == BadRequest) {
		printf("XI2 not available. Server supports %d.%d\n", major, minor);
		exit(-1);
	}

}

static void grabbing_set_brush_color(Grabber * self, char * color) {

	assert(self);

	if (color) {

		if (strcmp(color, "") == 0)
			self->brush_image = &brush_image_blue;
		else if (strcmp(color, "green") == 0)
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
 */
static ActiveWindowInfo * get_active_window_info(Display* dpy, Window win) {

	int ret, val;

	char *win_title;
	ret = fetch_window_title(dpy, win, &win_title);

	ActiveWindowInfo *ans = malloc(sizeof(ActiveWindowInfo));
	bzero(ans, sizeof(ActiveWindowInfo));

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
 */
static Window get_parent_window(Display *dpy, Window w) {
	Window root_return, parent_return, *child_return;
	unsigned int nchildren_return;
	int ret;
	ret = XQueryTree(dpy, w, &root_return, &parent_return, &child_return, &nchildren_return);

	return parent_return;
}

static double get_time(void) {
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec / 1000000.0;
}

#define SHM_SYNAPTICS 23947
typedef struct _SynapticsSHM {
	int version; /* Driver version */

	/* Current device state */
	int x, y; /* actual x, y coordinates */
	int z; /* pressure value */
	int numFingers; /* number of fingers */
	int fingerWidth; /* finger width value */
	int left, right, up, down; /* left/right/up/down buttons */
	Bool multi[8];
	Bool middle;
} SynapticsSHM;

static int synaptics_shm_is_equal(SynapticsSHM * s1, SynapticsSHM * s2) {
	int i;

	if ((s1->x != s2->x) || (s1->y != s2->y) || (s1->z != s2->z)
			|| (s1->numFingers != s2->numFingers) || (s1->fingerWidth != s2->fingerWidth)
			|| (s1->left != s2->left) || (s1->right != s2->right) || (s1->up != s2->up)
			|| (s1->down != s2->down) || (s1->middle != s2->middle))
		return 0;

	for (i = 0; i < 8; i++)
		if (s1->multi[i] != s2->multi[i])
			return 0;

	return 1;
}

void grabbing_xinput_grab(Grabber * self) {

	int count = XScreenCount(self->dpy);

	int screen;
	for (screen = 0; screen < count; screen++) {

		Window rootwindow = RootWindow(self->dpy, screen);

		if (self->is_direct_touch) {

			if (!self->button) {
				self->button = 1;
			}

			unsigned char mask_data[2] = { 0, };
			XISetMask(mask_data, XI_ButtonPress);
			XISetMask(mask_data, XI_Motion);
			XISetMask(mask_data, XI_ButtonRelease);
			XIEventMask mask = { XIAllDevices, sizeof(mask_data), mask_data };

			int status = XIGrabDevice(self->dpy, self->deviceid, rootwindow,
			CurrentTime, None,
			GrabModeAsync,
			GrabModeAsync, False, &mask);

		} else {

			if (!self->button) {
				self->button = 3;
			}

			unsigned char mask_data[2] = { 0, };
			XISetMask(mask_data, XI_ButtonPress);
			XISetMask(mask_data, XI_Motion);
			XISetMask(mask_data, XI_ButtonRelease);
			XIEventMask mask = { XIAllDevices, sizeof(mask_data), mask_data };

			XIGrabModifiers mods = { XIAnyModifier };
			int res = XIGrabButton(self->dpy, self->deviceid, self->button, rootwindow, None,
			GrabModeAsync, GrabModeAsync, False, &mask, 1, &mods);

		}

	}

}

void grabbing_xinput_ungrab(Grabber * self) {

	int count = XScreenCount(self->dpy);

	int screen;
	for (screen = 0; screen < count; screen++) {

		Window rootwindow = RootWindow(self->dpy, screen);

		/* select on the window */
		//XISelectEvents(display, w, &eventmask, 1);
		if (self->is_direct_touch) {

			int status = XIUngrabDevice(self->dpy, self->deviceid, CurrentTime);

		} else {
			XIGrabModifiers modifiers[4] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
			XIGrabModifiers mods = { XIAnyModifier };
			XIUngrabButton(self->dpy, self->deviceid, self->button, rootwindow, 1, &mods);
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

	self->fine_direction_sequence[0] = '\0';
	self->rought_direction_sequence[0] = '\0';

	self->old_x = new_x;
	self->old_y = new_y;

	self->rought_old_x = new_x;
	self->rought_old_y = new_y;

	if (!self->without_brush) {

		XFlush(self->dpy);
		backing_save(&(self->backing), new_x - self->brush.image_width,
				new_y - self->brush.image_height);
		brush_draw(&(self->brush), self->old_x, self->old_y);

	}
	return;
}

/*
 * Return the focused window at the given display.
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
	}
	XFlush(dpy);
	return;
}

/**
 * Obtém o resultado dos dois algoritmos de captura de movimentos, e envia para serem processadas.
 */
Capture * grabbing_end_movement(Grabber * self, int new_x, int new_y) {

	Capture * result = NULL;

	self->started = 0;

	// if is drawing
	if (!self->without_brush) {
		backing_restore(&(self->backing));
		XFlush(self->dpy);
	};

	if ((strlen(self->rought_direction_sequence) == 0)
			&& (strlen(self->fine_direction_sequence) == 0)) {

		if (!(self->synaptics)) {

			printf("Emulating click\n");

			// temporary ungrab button
			grabbing_xinput_ungrab(self);

			// emulate the click
			mouse_click(self->dpy, self->button, new_x, new_y);

			// restart grabbing
			grabbing_xinput_grab(self);

		}

	} else {

		int sequences_count = 2;
		char ** sequences = malloc(sizeof(char *) * sequences_count);

		sequences[0] = self->fine_direction_sequence;
		sequences[1] = self->rought_direction_sequence;

		ActiveWindowInfo * focused_window = get_active_window_info(self->dpy, get_focused_window(self->dpy));

		result = malloc(sizeof(Capture));

		result->expression_count = sequences_count;
		result->expression_list = sequences;
		result->active_window_info = focused_window;

	}

	return result;
}

static char get_fine_direction_from_deltas(int x_delta, int y_delta) {

	if ((x_delta == 0) && (y_delta == 0)) {
		return stroke_representations[NONE];
	}

	// check if the movement is near main axes
	if ((x_delta == 0) || (y_delta == 0) || (fabs((float) x_delta / (float) y_delta) > 3)
			|| (fabs((float) y_delta / (float) x_delta) > 3)) {

		// x axe
		if (abs(x_delta) > abs(y_delta)) {

			if (x_delta > 0) {
				return stroke_representations[RIGHT];
			} else {
				return stroke_representations[LEFT];
			}

			// y axe
		} else {

			if (y_delta > 0) {
				return stroke_representations[DOWN];
			} else {
				return stroke_representations[UP];
			}

		}

		// diagonal axes
	} else {

		if (y_delta < 0) {
			if (x_delta < 0) {
				return stroke_representations[SEVEN];
			} else if (x_delta > 0) { // RIGHT
				return stroke_representations[NINE];
			}
		} else if (y_delta > 0) { // DOWN
			if (x_delta < 0) { // RIGHT
				return stroke_representations[ONE];
			} else if (x_delta > 0) {
				return stroke_representations[THREE];
			}
		}

	}

	return stroke_representations[NONE];

}

static char get_direction_from_deltas(int x_delta, int y_delta) {

	if (abs(y_delta) > abs(x_delta)) {
		if (y_delta > 0) {
			return stroke_representations[DOWN];
		} else {
			return stroke_representations[UP];
		}

	} else {
		if (x_delta > 0) {
			return stroke_representations[RIGHT];
		} else {
			return stroke_representations[LEFT];
		}

	}

}

static void movement_add_direction(char* stroke_sequence, char direction) {
	// grab stroke
	int len = strlen(stroke_sequence);
	if ((len == 0) || (stroke_sequence[len - 1] != direction)) {

		if (len < MAX_STROKES_PER_CAPTURE) {

			stroke_sequence[len] = direction;
			stroke_sequence[len + 1] = '\0';

		}

	}
}

static void grabbing_update_movement(Grabber * self, int new_x, int new_y) {

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

	if ((abs(x_delta) > self->delta_min) || (abs(y_delta) > self->delta_min)) {

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

	if (self->delta_min * self->delta_min < square_distance_2) {
		// grab stroke

		movement_add_direction(self->rought_direction_sequence, rought_direction);

		// reset start position
		self->rought_old_x = new_x;
		self->rought_old_y = new_y;
	}

	return;
}

static void free_grabbed(Capture * free_me) {
	assert(free_me);
	free(free_me->active_window_info);
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
	}
	return 0;
}

Grabber * grabber_init(char * device_name, int button, int without_brush, int print_devices, char * brush_color, int verbose) {

	Grabber * self = malloc(sizeof(Grabber));
	bzero(self, sizeof(Grabber));

	grabber_open_display(self);

	grabbing_set_brush_color(self, brush_color);

	self->button = button;
	self->without_brush = without_brush;
	self->verbose = verbose;
	self->devicename = device_name;

	if (!self->devicename) {
		self->devicename = "Virtual core pointer";
	}

	if (strcasecmp(self->devicename, "SYNAPTICS") == 0) {
		self->synaptics = 1;
		self->delta_min = 200;
	} else {
		self->synaptics = 0;
		self->delta_min = 30;
	}

	int ndevices;
	int i;

	XIDeviceInfo * device;
	XIDeviceInfo * devices;

	int deviceid = -1;

	devices = XIQueryDevice(self->dpy, XIAllDevices, &ndevices);

	if (print_devices) {
		printf("\nXInput Devices:\n");
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

				if (strcasecmp(device->name, self->devicename) == 0) {
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

		printf("\nExperimental multitouch driver:\n");

		printf("   'SYNAPTICS'\n");
		printf("\nRun  mygestures -d 'DEVICE_NAME' to choose a device.\n");

	}

	XIFreeDeviceInfo(devices);

	if (print_devices) {
		exit(0);
	}

	self->fine_direction_sequence = (char *) malloc(sizeof(char) * (MAX_STROKES_PER_CAPTURE + 1));
	self->fine_direction_sequence[0] = '\0';

	self->rought_direction_sequence = (char *) malloc(sizeof(char) * (MAX_STROKES_PER_CAPTURE + 1));
	self->rought_direction_sequence[0] = '\0';

	int err = 0;
	int scr = DefaultScreen(self->dpy);

	if (!(self->synaptics)) {
		XAllowEvents(self->dpy, AsyncBoth, CurrentTime);
	}

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

/*
 * Minimum and maximum values for scroll_button_repeat
 */
//#define SBR_MIN 10
//#define SBR_MAX 1000
/** Init and return SHM area or NULL on error */
static SynapticsSHM *
grabber_synaptics_shm_init() {
	SynapticsSHM *synshm = NULL;
	int shmid = 0;

	if ((shmid = shmget(SHM_SYNAPTICS, sizeof(SynapticsSHM), 0)) == -1) {
		if ((shmid = shmget(SHM_SYNAPTICS, 0, 0)) == -1)
			fprintf(stderr, "Can't access shared memory area. SHMConfig disabled?\n");
		else
			fprintf(stderr, "Incorrect size of shared memory area. Incompatible driver version?\n");
	} else if ((synshm = (SynapticsSHM *) shmat(shmid, NULL, SHM_RDONLY)) == NULL)
		perror("shmat");

	return synshm;
}

void grabber_synaptics_loop(Grabber * self, Configuration * conf) {

	printf("\nMygestures is running on 3 fingers multitouch synaptics driver.\n");
	printf("Run `mygestures -l` to list other devices.\n");
	printf("\n");

	SynapticsSHM *synshm = NULL;

	synshm = grabber_synaptics_shm_init();
	if (!synshm) {
		printf(" You will need a patched synaptics driver with SHM enabled.\n");
		printf(" Take a look at https://github.com/Chosko/xserver-xorg-input-synaptics\n");
		return;
	}

	int delay = 10;

	SynapticsSHM old;
	double t0 = get_time();

	memset(&old, 0, sizeof(SynapticsSHM));
	old.x = -1; /* Force first equality test to fail */

	int max_fingers = 0;

	while (!self->shut_down) {

		SynapticsSHM cur = *synshm;

		if (!synaptics_shm_is_equal(&old, &cur)) {

			int delay = 10;

			// release
			if (cur.numFingers == 0 && max_fingers >= 3) {

				/// energy economy
				int delay = 50;

				if (self->verbose) {
					printf("%8.3f  %4d %4d %3d %d %2d %2d %d %d %d %d  %d%d%d%d%d%d%d%d\n",
							get_time() - t0, cur.x, cur.y, cur.z, cur.numFingers, cur.fingerWidth,
							cur.left, cur.right, cur.up, cur.down, cur.middle, cur.multi[0],
							cur.multi[1], cur.multi[2], cur.multi[3], cur.multi[4], cur.multi[5],
							cur.multi[6], cur.multi[7]);
					printf("stopped\n");
				}

				// reset max fingers
				max_fingers = 0;

				Capture * grab = grabbing_end_movement(self, old.x, old.y);

				if (grab) {

					printf("\n");
					printf("     Window title: \"%s\"\n", grab->active_window_info->title);
					printf("     Window class: \"%s\"\n", grab->active_window_info->class);

					Gesture * gest = configuration_process_gesture(conf, grab);

					if (gest) {
						printf("     Movement '%s' matched gesture '%s' on context '%s'\n",
								gest->movement->name, gest->name, gest->context->name);

						int j = 0;

						for (j = 0; j < gest->action_count; ++j) {
							Action * a = gest->action_list[j];
							printf("     Executing action: %s %s\n", get_action_name(a->type),
									a->original_str);
							execute_action(self->dpy, a, get_focused_window(self->dpy));

						}

					} else {

						for (int i = 0; i < grab->expression_count; ++i) {
							char * movement = grab->expression_list[i];
							printf("     Sequence '%s' does not match any known movement.\n",
									movement);
						}

					}

					printf("\n");

					free_grabbed(grab);

				}

				//// movement

			} else if (cur.numFingers >= 3 && max_fingers >= 3) {

				if (self->verbose) {
					printf("%8.3f  %4d %4d %3d %d %2d %2d %d %d %d %d  %d%d%d%d%d%d%d%d\n",
							get_time() - t0, cur.x, cur.y, cur.z, cur.numFingers, cur.fingerWidth,
							cur.left, cur.right, cur.up, cur.down, cur.middle, cur.multi[0],
							cur.multi[1], cur.multi[2], cur.multi[3], cur.multi[4], cur.multi[5],
							cur.multi[6], cur.multi[7]);
				}

				grabbing_update_movement(self, cur.x, cur.y);

				//// got > 3 fingers
			} else if (cur.numFingers >= 3 && max_fingers < 3) {

				if (self->verbose) {

					printf("%8.3f  %4d %4d %3d %d %2d %2d %d %d %d %d  %d%d%d%d%d%d%d%d\n",
							get_time() - t0, cur.x, cur.y, cur.z, cur.numFingers, cur.fingerWidth,
							cur.left, cur.right, cur.up, cur.down, cur.middle, cur.multi[0],
							cur.multi[1], cur.multi[2], cur.multi[3], cur.multi[4], cur.multi[5],
							cur.multi[6], cur.multi[7]);

				}

				max_fingers = max_fingers + 1;

				if (max_fingers >= 3) {

					if (self->verbose) {
						printf("started\n");
					}

					grabbing_start_movement(self, cur.x, cur.y);

				}

			}

			old = cur;
		}
		usleep(delay * 1000);
	}

}

void grabber_xinput_loop(Grabber * self, Configuration * conf) {

	XEvent ev;

	grabbing_xinput_grab(self);

	printf("\n");
	if (self->is_direct_touch) {
		printf("\nMygestures is running on device '%s' touch.\nDraw a gesture by touching it.\n",
				self->devicename);
	} else {
		printf(
				"\nMygestures is running on device '%s' button %d.\nDraw a gesture pressing this button.\n",
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
				grabbing_update_movement(self, data->root_x, data->root_y);
				break;

			case XI_ButtonPress:
				data = (XIDeviceEvent*) ev.xcookie.data;
				grabbing_start_movement(self, data->root_x, data->root_y);
				break;

			case XI_ButtonRelease:
				data = (XIDeviceEvent*) ev.xcookie.data;
				Capture * grab = grabbing_end_movement(self, data->root_x, data->root_y);

				if (grab) {

					printf("\n");
					printf("     Window title: \"%s\"\n", grab->active_window_info->title);
					printf("     Window class: \"%s\"\n", grab->active_window_info->class);

					Gesture * gest = configuration_process_gesture(conf, grab);

					if (gest) {
						printf("     Movement '%s' matched gesture '%s' on context '%s'\n",
								gest->movement->name, gest->name, gest->context->name);

						int j = 0;

						for (j = 0; j < gest->action_count; ++j) {
							Action * a = gest->action_list[j];
							printf("     Executing action: %s %s\n", get_action_name(a->type),
									a->original_str);
							execute_action(self->dpy, a, get_focused_window(self->dpy));
						}

					} else {

						for (int i = 0; i < grab->expression_count; ++i) {
							char * movement = grab->expression_list[i];
							printf("     Sequence '%s' does not match any known movement.\n",
									movement);
						}

					}

					printf("\n");

					free_grabbed(grab);

				}

				break;

			}
		}
		XFreeEventData(self->dpy, &ev.xcookie);

	}

	grabbing_xinput_ungrab(self);
}

void grabber_loop(Grabber * self, Configuration * conf) {
	if (self->synaptics) {
		grabber_synaptics_loop(self, conf);
	} else {
		grabber_xinput_loop(self, conf);
	}
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

