#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#include <getopt.h>
#include <unistd.h>

#include "assert.h"
#include "gestos.h"

#include "xinput-grabber.h"

int mousegestures_loop(Gestos *gestos)
{

	int pid = fork();

	if (pid == 0)
	{

		gestos->grabbers++;

		XInputGrabber *xinput;

		xinput = grabber_xinput_new(gestos->device_name, gestos->button);

		if (gestos->list_devices_flag)
		{
			grabber_xinput_list_devices(xinput);
			exit(0);
		}

		grabber_xinput_loop(xinput, gestos->gestures);

		exit(0);
	}

	return pid;
}
