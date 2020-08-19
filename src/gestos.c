#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "assert.h"
#include "signal.h"
#include <sys/wait.h>

#include "gestos.h"
#include "gestures.h"
#include "xinput-grabber.h"
#include "libinput-grabber.h"

// to control child proceses
int CHILD_COUNT = 0;

static void gestos_usage()
{

	printf("\n");
	printf(" Usage: gestos [OPTIONS]\n");
	printf("\n");
	printf(" GENERAL OPTIONS:\n");
	printf("  -c, --config <CONFIG_FILE> : Gestures configuration file.\n");
	printf("  -d, --device <DEVICENAME>  : The device to grab (only if you want to restrict gestures to this device)\n");
	printf("  -l, --device-list          : Print all available devices an exit.\n");
	printf("  -h, --help                 : Help\n");
	printf("\n");
	printf(" TOUCH GESTURES OPTIONS:\n");
	printf("  -f, --fingers <FINGERS>    : Default: 3 fingers to trigger gestures\n");
	printf("\n");
	printf(" MOUSE GESTURES OPTIONS:\n");
	printf("  -b, --button <BUTTON>      : The mouse button to be pressed. Default button: 3 (right button))\n");
	printf("\n");
}

static void gestos_process_arguments(Gestos *self, int argc, char *const *argv)
{

	char opt;
	static struct option opts[] = {
		{"device", required_argument, 0, 'd'},
		{"config", required_argument, 0, 'c'},
		{"fingers", required_argument, 0, 'f'},
		{"button", required_argument, 0, 'b'},
		{"help", no_argument, 0, 'h'},
		{"list-devices", no_argument, 0, 'l'},
		{0, 0, 0, 0}};

	/* read params */

	while (1)
	{
		opt = getopt_long(argc, argv, "b:f:c:d:hl", opts, NULL);
		if (opt == -1)
			break;

		switch (opt)
		{

		case 'c':
			self->config_file = strdup(optarg);
			break;

		case 'l':
			self->list_devices_flag = 1;
			break;

		case 'f':
			self->fingers = atoi(optarg);
			break;

		case 'b':
			self->button = atoi(optarg);
			break;

		case 'h':
			gestos_usage();
			exit(0);
			break;

		case 'd':
			self->device_name = strdup(optarg);
			break;
		}
	}

	if (!(self->device_name))
	{
		self->device_name = "";
	}
}

Gestos *gestos_new()
{
	Gestos *self = malloc(sizeof(Gestos));
	bzero(self, sizeof(Gestos));
	return self;
}

void child_exit()
{

	if (CHILD_COUNT--)
	{
		printf("All processes gone. Exiting\n");
		exit(0);
	}

	wait(NULL);
}

int gestos_mousegestures_loop(Gestos *gestos)
{

	int pid = fork();

	if (pid == 0)
	{

		CHILD_COUNT++;

		XInputGrabber *xinput;

		// mousegestures handing will make use of xinput.
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

void gestos_load_gestures(Gestos *self)
{
	Gestures *gestures = gestures_new();
	gestures_load_from_file(gestures, self->config_file);
	self->gestures = gestures;
}

int gestos_touchgestures_loop(Gestos *gestos)
{

	int pid = fork();

	if (pid == 0)
	{

		CHILD_COUNT++;

		// touchgestures handing will make use of libinput.
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

int main(int argc, char *const *argv)
{

	Gestos *self = gestos_new();

	signal(SIGCHLD, child_exit);

	gestos_process_arguments(self, argc, argv);

	gestos_load_gestures(self);

	gestos_touchgestures_loop(self);
	gestos_mousegestures_loop(self);

	wait(NULL);
}
