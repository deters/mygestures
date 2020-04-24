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
#include <X11/extensions/XTest.h>	/* emulating device events */

#include "mygestures.h"

#include "xinput-grabber.h"

static void mouse_click(Display *display, int button, int x, int y)
{

	XTestFakeMotionEvent(display, DefaultScreen(display), x, y, 0);
	XTestFakeButtonEvent(display, button, True, CurrentTime);
	XTestFakeButtonEvent(display, button, False, CurrentTime);
}

void grabbing_xinput_grab_start(XInputGrabber *self)
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

void grabbing_xinput_grab_stop(XInputGrabber *self)
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

static void grabber_xinput_open_devices(XInputGrabber *self, int verbose)
{

	int ndevices;
	int i;
	XIDeviceInfo *device;
	XIDeviceInfo *devices;

	//assert(self->dpy);

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
			if (strcmp(self->devicename, "") == 0)
			{
				if (verbose)
				{
					printf("   [x] '%s'\n", device->name);
				}

				self->deviceid = device->deviceid;
				self->devicename = device->name;
			}
			break;
		case XISlavePointer:
		case XIFloatingSlave:

			if (verbose)
			{

				if (strcasecmp(device->name, self->devicename) == 0)
				{
					self->deviceid = device->deviceid;
					self->is_direct_touch = get_touch_status(device);
					printf("   [x] '%s'\n", device->name);
				}
				else
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

static void grabber_open_display(XInputGrabber *self)
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

XInputGrabber *grabber_xinput_new(char *device_name, int button)
{

	XInputGrabber *self = malloc(sizeof(XInputGrabber));
	bzero(self, sizeof(XInputGrabber));

	assert(device_name);
	assert(button);

	self->devicename = device_name;
	self->synaptics = 0;
	self->delta_min = 30;
	self->button = button;

	return self;
}

char *get_device_name_from_event(XInputGrabber *self, XIDeviceEvent *data)
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

void grabber_xinput_list_devices(XInputGrabber *self)
{
	grabber_open_display(self);
	grabber_xinput_open_devices(self, True);
};

void grabber_xinput_loop(XInputGrabber *self, Mygestures *mygestures)
{

	XEvent ev;

	assert(self);
	assert(mygestures);

	grabber_open_display(self);

	grabber_xinput_open_devices(self, True);

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
				mygestures_update_movement(mygestures, data->root_x, data->root_y, self->delta_min);
				break;

			case XI_ButtonPress:
				data = (XIDeviceEvent *)ev.xcookie.data;
				mygestures_start_movement(mygestures, data->root_x, data->root_y, self->delta_min);
				break;

			case XI_ButtonRelease:
				data = (XIDeviceEvent *)ev.xcookie.data;

				char *device_name = get_device_name_from_event(self, data);

				grabbing_xinput_grab_stop(self);
				int status = grabbing_end_movement(mygestures, 0,
												   device_name, mygestures);

				if (!status)
				{
					printf("\nEmulating click\n");

					//grabbing_xinput_grab_stop(self);
					mouse_click(self->dpy, self->button, data->root_x, data->root_y);
					//grabbing_xinput_grab_start(self);
				}

				grabbing_xinput_grab_start(self);
				break;
			}
		}
		XFreeEventData(self->dpy, &ev.xcookie);
	}
}

char *grabber_get_device_name(XInputGrabber *self)
{
	return self->devicename;
}

// void grabber_finalize(Grabber *self)
// {
// 	if (self->brush_image)
// 	{
// 		brush_deinit(&(self->brush));
// 		backing_deinit(&(self->backing));
// 	}

// 	XCloseDisplay(self->dpy);
// 	return;
// }
