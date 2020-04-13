/*
 *   Copyright 2002-2004 Peter Osterlund <petero2@telia.com>
 *   Copyright 2016      Lucas Augusto Deters
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <sys/shm.h> /* needed for synaptics */
#include <X11/Xlib.h>
#include <X11/extensions/XI.h>
#include <X11/extensions/XInput.h>

#include <math.h>

#include <sys/time.h>

#include "grabbing-synaptics.h"

#define SHM_SYNAPTICS 23947

#define SYNAPTICS_PROP_TAP_ACTION "Synaptics Tap Action"

SynapticsGrabber *grabber_synaptics_new()
{

	SynapticsGrabber *self = malloc(sizeof(SynapticsGrabber));
	bzero(self, sizeof(SynapticsGrabber));

	self->delta_min = 100;

	//assert(button);

	self->devicename = "synaptics";
	//grabber_set_button(self, button);

	return self;
}

static XDevice *
dp_get_device(Display *dpy)
{

	assert(dpy);

	XDevice *dev = NULL;
	XDeviceInfo *info = NULL;
	int ndevices = 0;
	Atom touchpad_type = 0;
	Atom synaptics_property = 0;
	Atom *properties = NULL;
	int nprops = 0;
	int error = 0;

	touchpad_type = XInternAtom(dpy, XI_TOUCHPAD, True);
	synaptics_property = XInternAtom(dpy, SYNAPTICS_PROP_TAP_ACTION, True);
	info = XListInputDevices(dpy, &ndevices);

	while (ndevices--)
	{
		if (info[ndevices].type == touchpad_type)
		{
			dev = XOpenDevice(dpy, info[ndevices].id);
			if (!dev)
			{
				fprintf(stderr, "Failed to open device '%s'.\n",
						info[ndevices].name);
				error = 1;
				goto unwind;
			}

			properties = XListDeviceProperties(dpy, dev, &nprops);
			if (!properties || !nprops)
			{
				fprintf(stderr, "No properties on device '%s'.\n",
						info[ndevices].name);
				error = 1;
				goto unwind;
			}

			while (nprops--)
			{
				if (properties[nprops] == synaptics_property)
					break;
			}
			if (!nprops)
			{
				fprintf(stderr, "No synaptics properties on device '%s'.\n",
						info[ndevices].name);
				error = 1;
				goto unwind;
			}

			break; /* Yay, device is suitable */
		}
	}

unwind:
	XFree(properties);
	XFreeDeviceList(info);
	if (!dev)
		fprintf(stderr, "Unable to find a synaptics device.\n");
	else if (error && dev)
	{
		XCloseDevice(dpy, dev);
		dev = NULL;
	}
	return dev;
}

typedef struct _SynapticsSHM
{
	int version; /* Driver version */

	/* Current device state */
	int x, y;				   /* actual x, y coordinates */
	int z;					   /* pressure value */
	int numFingers;			   /* number of fingers */
	int fingerWidth;		   /* finger width value */
	int left, right, up, down; /* left/right/up/down buttons */
	Bool multi[8];
	Bool middle;
} SynapticsSHM;

static int synaptics_shm_is_equal(SynapticsSHM *s1, SynapticsSHM *s2)
{
	int i;

	if ((s1->x != s2->x) || (s1->y != s2->y) || (s1->z != s2->z) || (s1->numFingers != s2->numFingers) || (s1->fingerWidth != s2->fingerWidth) || (s1->left != s2->left) || (s1->right != s2->right) || (s1->up != s2->up) || (s1->down != s2->down) || (s1->middle != s2->middle))
		return 0;

	for (i = 0; i < 8; i++)
		if (s1->multi[i] != s2->multi[i])
			return 0;

	return 1;
}

/** Init and return SHM area or NULL on error */
static SynapticsSHM *
grabber_synaptics_shm_init(int debug)
{
	SynapticsSHM *synshm = NULL;
	int shmid = shmget(SHM_SYNAPTICS, sizeof(SynapticsSHM), 0);

	if (shmid == -1)
	{

		shmid = shmget(SHM_SYNAPTICS, 0, 0);

		if (shmid == -1)
		{
			if (debug)
			{
				printf(
					"Can't access shared memory area. SHMConfig disabled?\n");
			}
		}
		else
		{
			if (debug)
			{
				printf(
					"Incorrect size of shared memory area. Incompatible driver version?\n");
			}
		}
	}
	else if ((synshm = (SynapticsSHM *)shmat(shmid, NULL, SHM_RDONLY)) == NULL)
	{
		if (debug)
		{
			perror("shmat");
		}
	}

	return synshm;
}

void syn_print(const SynapticsSHM *cur)
{
	printf("%4d %4d %3d %d %2d %2d %d %d %d %d  %d%d%d%d%d%d%d%d\n", cur->x,
		   cur->y, cur->z, cur->numFingers, cur->fingerWidth, cur->left,
		   cur->right, cur->up, cur->down, cur->middle, cur->multi[0],
		   cur->multi[1], cur->multi[2], cur->multi[3], cur->multi[4],
		   cur->multi[5], cur->multi[6], cur->multi[7]);
}

void synaptics_disable_3fingers_tap(SynapticsGrabber *self, XDevice *dev)
{

	assert(self->dpy);
	assert(dev);

	Atom prop, type;
	int format;
	unsigned long nitems, bytes_after;
	unsigned char *data = NULL;
	prop = XInternAtom(self->dpy, SYNAPTICS_PROP_TAP_ACTION, True);

	/* get current configuration */

	XGetDeviceProperty(self->dpy, dev, prop, 0, 1000, False, AnyPropertyType,
					   &type, &format, &nitems, &bytes_after, &data);
	char *b = (char *)data;
	int offset = 6; // the position of 3TAP_FINGER inside config

	/* change configuration if needed */

	if (b[offset] != 0)
	{
		b[offset] = 0; //rint(val);

		XChangeDeviceProperty(self->dpy, dev, prop, type, format,
							  PropModeReplace, data, nitems);
		XFlush(self->dpy);
	}
}

void grabber_synaptics_loop(SynapticsGrabber *self, Mygestures *mygestures)
{

	self->dpy = mygestures->dpy;

	XDevice *dev = NULL;

	Atom synaptics_property = 0;

	assert(self->dpy);
	assert(mygestures);

	synaptics_property = XInternAtom(self->dpy, SYNAPTICS_PROP_TAP_ACTION, True);
	if (!synaptics_property)
	{
		fprintf(stderr, "Synaptics driver not found. Multitouch gestures disabled. \n");
		return;
	}

	dev = dp_get_device(self->dpy);

	if (!dev)
	{
		printf("No synaptics touchpad detected.\n");
		return;
	}

	synaptics_disable_3fingers_tap(self, dev);

	SynapticsSHM *synshm = NULL;
	synshm = grabber_synaptics_shm_init(0);

	printf("\nSynaptics Driver (with Shared Memory enabled):\n");
	printf("   [x] 'synaptics'\n");

	if (!synshm)
	{
		printf(
			"Your Synaptics driver does not have shared memory access, so multitouch gestures will not work on your touchpad.\n"
			"Take a look at https://github.com/Chosko/xserver-xorg-input-synaptics if you want to enable SynapticsSHM.\n");
		return;
	}

	int delay = 10;

	SynapticsSHM old;

	memset(&old, 0, sizeof(SynapticsSHM));
	old.x = -1; /* Force first equality test to fail */

	int max_fingers = 0;

	while (!self->shut_down)
	{

		SynapticsSHM cur = *synshm;

		if (!synaptics_shm_is_equal(&old, &cur))
		{

			delay = 10;

			// release
			if (cur.numFingers >= 3 && max_fingers >= 3)
			{

				mygestures_update_movement(mygestures, cur.x, cur.y, self->delta_min);

				//// got > 3 fingers
			}
			else if (cur.numFingers == 0 && max_fingers >= 3)
			{

				if (self->verbose)
				{
					syn_print(&cur);
					printf("stopped	\n");
				}

				// reset max fingers
				max_fingers = 0;

				grabbing_end_movement(mygestures, old.x, old.y, "Synaptics", mygestures);

				/// energy economy
				delay = 50;
			}
			else if (cur.numFingers >= 3 && max_fingers < 3)
			{

				if (self->verbose)
				{

					syn_print(&cur);
				}

				max_fingers = max_fingers + 1;

				if (max_fingers >= 3)
				{

					if (self->verbose)
					{
						printf("started\n");
					}

					mygestures_start_movement(mygestures, cur.x, cur.y, self->delta_min);
				}
			}

			//// movement
		}

		usleep(delay * 1000);

		old = cur;
	}
}
