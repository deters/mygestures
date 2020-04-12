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

#include <assert.h>

#include <X11/extensions/XInput2.h> /* capturing device events */

#include "mygestures.h"

#include "grabbing-xinput.h"
#include "grabbing-synaptics.h"

static void grabber_open_display(Grabber *self)
{

	self->dpy = XOpenDisplay(NULL);

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

static void grabber_init_drawing(Grabber *self)
{

	assert(self->dpy);

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

void grabbing_xinput_grab_start(Grabber *self)
{

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

			int err = XIGrabDevice(self->dpy, self->deviceid, rootwindow,
								   CurrentTime, None,
								   GrabModeAsync,
								   GrabModeAsync, False, &mask);

			if (err)
			{

				char *s;

				switch (err)
				{
				case GrabNotViewable:
					s = "not viewable";
					break;
				case AlreadyGrabbed:
					s = "already grabbed";
					break;
				case GrabFrozen:
					s = "grab frozen";
					break;
				case GrabInvalidTime:
					s = "invalid time";
					break;
				case GrabSuccess:
					return;
				default:
					printf("%s: grab error: %d\n", self->devicename, err);
					return;
				}
				printf("%s: grab error: %s\n", self->devicename, s);
			}
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

			int err = XIGrabButton(self->dpy, self->deviceid, self->button,
								   rootwindow, None,
								   GrabModeAsync, GrabModeAsync, False, &mask, nmods, mods);

			if (err)
			{

				char *s;

				switch (err)
				{
				case GrabNotViewable:
					s = "not viewable";
					break;
				case AlreadyGrabbed:
					s = "already grabbed";
					break;
				case GrabFrozen:
					s = "grab frozen";
					break;
				case GrabInvalidTime:
					s = "invalid time";
					break;
				case GrabSuccess:
					return;
				default:
					printf("%s: grab error: %d\n", self->devicename, err);
					return;
				}
				printf("%s: grab error: %s\n", self->devicename, s);
			}
		}
	}
}

void grabbing_xinput_grab_stop(Grabber *self)
{

	int count = XScreenCount(self->dpy);

	int screen;
	for (screen = 0; screen < count; screen++)
	{

		Window rootwindow = RootWindow(self->dpy, screen);

		if (self->is_direct_touch)
		{

			XIUngrabDevice(self->dpy, self->deviceid, CurrentTime);
		}
		else
		{
			XIGrabModifiers mods = {
				XIAnyModifier};
			XIUngrabButton(self->dpy, self->deviceid, self->button, rootwindow,
						   1, &mods);
		}
	}
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

Grabber *grabber_new(char *device_name, int button)
{

	Grabber *self = malloc(sizeof(Grabber));
	bzero(self, sizeof(Grabber));

	self->fine_direction_sequence = malloc(sizeof(char) * 30);
	self->rought_direction_sequence = malloc(sizeof(char) * 30);

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
		device_name = device->name;
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
	if (self->brush_image)
	{
		brush_deinit(&(self->brush));
		backing_deinit(&(self->backing));
	}

	XCloseDisplay(self->dpy);
	return;
}
