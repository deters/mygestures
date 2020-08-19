#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#include <getopt.h>
#include <unistd.h>

#include "assert.h"

#include "libinput-grabber.h"

int touch_nfingers = 3;
int touch_devices_flag;
int touch_list_device_flags;
char *touch_config_file;
char *touch_device_name = "";

static void touchgestures_usage()
{
	printf("Usage: touchgestures [OPTIONS]\n");
	printf("\n");
	//printf("CONFIG_FILE:\n");

	//char *default_file = mygestures. configuration_get_default_filename(mygestures);
	//printf(" Default: %s\n", default_file);
	//free(default_file);

	printf("\n");
	printf("OPTIONS:\n");
	printf(" -c, --config <CONFIG_FILE> : Gestures configuration file.\n");
	printf(" -d, --device <DEVICENAME>  : Device to grab. Default: the first detected touchpad.\n");
	printf(" -f, --fingers <FINGERS>    : How many fingers trigger the gesture. Default: 3\n");
	printf(" -l, --device-list          : Print all available devices an exit.\n");
	printf(" -h, --help                 : Help\n");
}

static void touchgestures_process_arguments(int argc, char *const *argv)
{

	char opt;
	static struct option opts[] = {
		{"device", required_argument, 0, 'd'},
		{"fingers", required_argument, 0, 'f'},
		{"config", required_argument, 0, 'c'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}};

	/* read params */

	while (1)
	{
		opt = getopt_long(argc, argv, "f:c:d:vhl", opts, NULL);
		if (opt == -1)
			break;

		switch (opt)
		{

		case 'f':
			touch_nfingers = atoi(optarg);
			break;

		case 'd':
			touch_device_name = strdup(optarg);
			break;

		case 'c':
			touch_config_file = strdup(optarg);
			break;

		case 'l':
			touch_list_device_flags = 1;
			break;

		case 'h':
			touchgestures_usage();
			exit(0);
			break;
		}
	}

	if (optind < argc)
	{
		touch_config_file = argv[optind++];
	}

	if (optind < argc)
	{
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		putchar('\n');
	}
}

int touchgestures_main(int argc, char *const *argv)
{

	touchgestures_process_arguments(argc, argv);

	Mygestures *mygestures = mygestures_new();

	mygestures_load_configuration(mygestures, touch_config_file);

	LibinputGrabber *libinput;

	touch_device_name = "";

	libinput = libinput_grabber_new(touch_device_name, touch_nfingers);

	if (touch_list_device_flags)
	{
		//printf("not implemented!\n");
		libinput_grabber_list_devices();
		//grabber_libinput_list_devices(libinput);
		exit(0);
	}

	libinput_grabber_loop(libinput, mygestures);

	exit(0);
}
