#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#include <getopt.h>
#include <unistd.h>

#include "assert.h"
#include "gestos.h"

#include "libinput-grabber.h"

int touchgestures_loop(Gestos *gestos)
{

	int pid = fork();

	if (pid == 0)
	{

		gestos->grabbers++;

		LibinputGrabber *libinput;

		if (gestos->list_devices_flag)
		{
			libinput_grabber_list_devices();
			exit(0);
		}

		libinput = libinput_grabber_new(gestos->device_name, gestos->fingers);

		libinput_grabber_loop(libinput, gestos->gestures);

		exit(0);
	}

	return pid;
}
