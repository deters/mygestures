#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "assert.h"

#include "mousegestures.h"
#include "touchgestures.h"
#include "gestos.h"
#include "gestures.h"

static void gestos_usage()
{
	printf("Usage: gestos [OPTIONS]\n");

	printf("\n");
	printf("GENERAL OPTIONS:\n");
	printf(" -c, --config <CONFIG_FILE> : Gestures configuration file.\n");
	printf(" -d, --device <DEVICENAME>  : Device to grab. Default: the first detected touchpad.\n");
	printf(" -l, --device-list          : Print all available devices an exit.\n");
	printf(" -h, --help                 : Help\n");

	printf("TOUCHGESTURES OPTIONS:\n");
	printf(" -f, --fingers <FINGERS>    : How many fingers will trigger the gesture. Default: 3\n");

	printf("MOUSEGESTURES OPTIONS:\n");
	printf(" -b, --button <BUTTON>      : Device to grab. Default: the first detected touchpad.\n");
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

void gestos_run(Gestos *self)
{
	Gestures *gestures = gestures_new();
	gestures_load_from_file(gestures, self->config_file);
	self->gestures = gestures;

	int pid = fork();

	if (pid == 0)
	{
		touchgestures_loop(self);
	}
	else
	{
		mousegestures_loop(self);
	}
}

int main(int argc, char *const *argv)
{

	Gestos *gestos = gestos_new();

	gestos_process_arguments(gestos, argc, argv);
	gestos_run(gestos);
}
