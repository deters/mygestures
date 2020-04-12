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
#include <signal.h>

#include <sys/types.h>

#include "assert.h"
#include "string.h"
#include "config.h"

#include "mygestures.h"
#include "main.h"

#include "grabbing-xinput.h"
#include "configuration.h"
#include "configuration_parser.h"
#include "drawing/drawing-brush-image.h"
#include "actions.h"

#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include <X11/extensions/XTest.h> /* emulating device events */

#include <sys/mman.h>
#include <sys/shm.h>

uint MAX_GRABBED_DEVICES = 10;

#ifndef MAX_STROKES_PER_CAPTURE
#define MAX_STROKES_PER_CAPTURE 63 /*TODO*/
#endif

struct shm_message
{
	int pid;
	int kill;
};

static struct shm_message *message;
static char *shm_identifier;

const char stroke_representations[] = {' ', 'L', 'R', 'U', 'D', '1', '3', '7',
									   '9'};

/*
 * Ask other instances with same unique_identifier to exit.
 */
void send_kill_message(char *device_name)
{

	assert(message);

	/* if shared message contains a PID, kill that process */
	if (message->pid > 0)
	{
		printf("Asking mygestures running on pid %d to exit..\n", message->pid);

		int running = message->pid;

		message->pid = getpid();
		message->kill = 1;

		int err = kill(running, SIGINT);

		if (err)
		{
			printf("Error sending kill message.\n");
		}

		/* give some time. ignore failing */
		usleep(100 * 1000); // 100ms
	}

	/* write own PID in shared memory */
	message->pid = getpid();
	message->kill = 0;
}

static void char_replace(char *str, char oldChar, char newChar)
{
	assert(str);
	char *strPtr = str;
	while ((strPtr = strchr(strPtr, oldChar)) != NULL)
		*strPtr++ = newChar;
}

void alloc_shared_memory(char *device_name, int button)
{

	char *sanitized_device_name = device_name;

	if (sanitized_device_name)
	{
		char_replace(sanitized_device_name, '/', '%');
	}
	else
	{
		sanitized_device_name = "";
	}

	int size = asprintf(&shm_identifier, "/mygestures_uid_%d_dev_%s_button_%d", getuid(),
						sanitized_device_name, button);

	if (size < 0)
	{
		printf("Error in asprintf at alloc_shared_memory\n");
		exit(1);
	}

	int shared_seg_size = sizeof(struct shm_message);
	int shmfd = shm_open(shm_identifier, O_CREAT | O_RDWR, 0600);

	//free(shm_identifier);

	if (shmfd < 0)
	{
		perror("In shm_open()");
		exit(shmfd);
	}
	int err = ftruncate(shmfd, shared_seg_size);

	if (err)
	{
		printf("Error truncating SHM variable\n");
	}

	message = (struct shm_message *)mmap(NULL, shared_seg_size,
										 PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

	if (message == NULL)
	{
		perror("In mmap()");
		exit(1);
	}
}

static void release_shared_memory()
{

	/*  If your head comes away from your neck, it's over! */

	if (shm_identifier)
	{

		if (shm_unlink(shm_identifier) != 0)
		{
			perror("In shm_unlink()");
			exit(1);
		}

		free(shm_identifier);
	}
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

void on_interrupt(int a)
{

	if (message->kill)
	{
		printf("\nMygestures on PID %d asked me to exit.\n", message->pid);
		// shared memory now belongs to the other process. will not be released
	}
	else
	{
		printf("\nReceived the interrupt signal.\n");
		release_shared_memory();
	}

	exit(0);
}

void on_kill(int a)
{
	//release_shared_memory();

	exit(0);
}

Mygestures *mygestures_new()
{

	Mygestures *self = malloc(sizeof(Mygestures));
	bzero(self, sizeof(Mygestures));

	self->device_list = malloc(sizeof(char *) * MAX_GRABBED_DEVICES);
	self->gestures_configuration = configuration_new();

	return self;
}

static void mygestures_load_configuration(Mygestures *self)
{

	if (self->custom_config_file)
	{
		configuration_load_from_file(self->gestures_configuration,
									 self->custom_config_file);
	}
	else
	{
		configuration_load_from_defaults(self->gestures_configuration);
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

static void grabber_set_brush_color(Grabber *self, char *brush_color)
{
	self->brush_image = get_brush_image(brush_color);
}

static void mygestures_grab_device(Mygestures *self, char *device_name)
{

	int p = fork();

	if (p != 0)
	{

		/* We are in the forked thread. Start grabbing a device */

		printf("Listening to device '%s'\n\n", device_name);

		alloc_shared_memory(device_name, self->trigger_button);

		Grabber *grabber = grabber_new(device_name, self->trigger_button);

		grabber_set_brush_color(grabber, self->brush_color);

		send_kill_message(device_name);

		signal(SIGINT, on_interrupt);
		signal(SIGKILL, on_kill);

		if (self->list_devices_flag)
		{
			grabber_list_devices(grabber);
		}
		else
		{
			grabber_loop(grabber, self->gestures_configuration);
		}
	}
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

	if (self->brush_image)
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
	if (self->brush_image)
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

// static Window get_window_under_pointer(Display *dpy)
// {

// 	Window root_return, child_return;
// 	int root_x_return, root_y_return;
// 	int win_x_return, win_y_return;
// 	unsigned int mask_return;
// 	XQueryPointer(dpy, DefaultRootWindow(dpy), &root_return, &child_return,
// 				  &root_x_return, &root_y_return, &win_x_return, &win_y_return,
// 				  &mask_return);

// 	Window w = child_return;
// 	Window parent_return;
// 	Window *children_return;
// 	unsigned int nchildren_return;
// 	XQueryTree(dpy, w, &root_return, &parent_return, &children_return,
// 			   &nchildren_return);

// 	return children_return[nchildren_return - 1];
// }

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

static void execute_action(Display *dpy, Action *action, Window focused_window)
{
	int id;

	assert(dpy);
	assert(action);
	assert(focused_window);

	switch (action->type)
	{
	case ACTION_EXECUTE:
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

	XAllowEvents(dpy, 0, CurrentTime);

	return;
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

static void mouse_click(Display *display, int button, int x, int y)
{

	XTestFakeMotionEvent(display, DefaultScreen(display), x, y, 0);
	XTestFakeButtonEvent(display, button, True, CurrentTime);
	XTestFakeButtonEvent(display, button, False, CurrentTime);
}

static void free_grabbed(Capture *free_me)
{
	assert(free_me);
	free(free_me->active_window_info);
	free(free_me);
}

/**
 *
 */
void grabbing_end_movement(Grabber *self, int new_x, int new_y,
						   char *device_name, Configuration *conf)
{

	Window focused_window = get_focused_window(self->dpy);
	Window target_window = focused_window;

	Capture *grab = NULL;

	self->started = 0;

	// if is drawing
	if (self->brush_image)
	{
		backing_restore(&(self->backing));
	};

	// if there is no gesture
	if ((strlen(self->rought_direction_sequence) == 0) && (strlen(self->fine_direction_sequence) == 0))
	{

		if (!(self->synaptics))
		{

			printf("\nEmulating click\n");

			//grabbing_xinput_grab_stop(self);
			mouse_click(self->dpy, self->button, new_x, new_y);
			//grabbing_xinput_grab_start(self);
		}
	}
	else
	{

		int expression_count = 2;
		char **expression_list = malloc(sizeof(char *) * expression_count);

		expression_list[0] = self->fine_direction_sequence;
		expression_list[1] = self->rought_direction_sequence;

		ActiveWindowInfo *window_info = get_active_window_info(self->dpy,
															   target_window);

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
				execute_action(self->dpy, a, target_window);
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
}

void mygestures_run(Mygestures *self)
{

	printf("%s\n\n", PACKAGE_STRING);
	/*
	 * Will not load configuration if it is only listing the devices.
	 */
	if (!self->list_devices_flag)
	{
		mygestures_load_configuration(self);
	}

	if (self->libinput)
	{
	}
	else
	{

		if (self->multitouch)
		{
			printf("Starting in multitouch mode.\n");
			mygestures_grab_device(self, "synaptics");
		}
		else
		{

			if (self->device_count)
			{
				/*
		 * Start grabbing any device passed via argument flags.
		 */
				for (int i = 0; i < self->device_count; ++i)
				{
					mygestures_grab_device(self, self->device_list[i]);
				}
			}
			else
			{

				printf("Selecting default xinput device.\n");
				mygestures_grab_device(self, "Virtual Core Pointer");
				/*
		 * If there where no devices in the argument flags, then grab the default devices.
		 */
			}
		}
	}
}
