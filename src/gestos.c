#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include "assert.h"

#include "xinput-grabber.h"
#include "libinput-grabber.h"
#include "gestures.h"

int trigger_button = 3;
int list_devices_flag;
char *config_file;
char *device_name = "";

int main(int argc, char *const *argv)
{

	Mygestures *mygestures = mygestures_new();

	mygestures_load_configuration(mygestures, config_file);

	XInputGrabber *xinput;
	LibinputGrabber *libinput;

	xinput = grabber_xinput_new(device_name, 3);
	libinput = libinput_grabber_new(device_name, 3);

	int pid = fork();

	if (pid == 0)
	{
		libinput_grabber_loop(libinput, mygestures);
	}
	else
	{
		grabber_xinput_loop(xinput, mygestures);
	}

	exit(0);
}
