#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#include <getopt.h>
#include <unistd.h>

#include "assert.h"

#include "xinput-grabber.h"

int mouse_trigger_button = 3;
int mouse_list_devices_flag;
char *mouse_config_file;
char *mouse_device_name = "";

static void mygestures_usage()
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
	printf(" -b, --button <BUTTON>      : Device to grab. Default: the first detected touchpad.\n");
	printf(" -l, --device-list          : Print all available devices an exit.\n");
	printf(" -h, --help                 : Help\n");
}

static void process_arguments(int argc, char *const *argv)
{

	char opt;
	static struct option opts[] = {
		{"device", required_argument, 0, 'd'},
		{"button", required_argument, 0, 'b'},
		{"config", required_argument, 0, 'c'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}};

	/* read params */

	while (1)
	{
		opt = getopt_long(argc, argv, "b:f:c:i:d:vhl", opts, NULL);
		if (opt == -1)
			break;

		switch (opt)
		{

		case 'b':
			mouse_trigger_button = atoi(optarg);
			break;

		case 'd':
			mouse_device_name = strdup(optarg);
			break;

		case 'c':
			mouse_config_file = strdup(optarg);
			break;

		case 'l':
			mouse_list_devices_flag = 1;
			break;

		case 'h':
			mygestures_usage();
			exit(0);
			break;
		}
	}

	if (optind < argc)
	{
		mouse_config_file = argv[optind++];
	}

	if (optind < argc)
	{
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		putchar('\n');
	}
}

int mousegestures_main(int argc, char *const *argv)
{

	process_arguments(argc, argv);

	Mygestures *mygestures = mygestures_new();

	mygestures_load_configuration(mygestures, mouse_config_file);

	XInputGrabber *xinput;

	xinput = grabber_xinput_new(mouse_device_name, mouse_trigger_button);

	if (mouse_list_devices_flag)
	{
		grabber_xinput_list_devices(xinput);
		exit(0);
	}

	grabber_xinput_loop(xinput, mygestures);

	exit(0);
}
