#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#include <getopt.h>
#include <unistd.h>

#include "assert.h"

#include "grabbing-libinput.h"

int nfingers = 3;
int list_devices_flag;
int visual = 0;
char *config_file;
char *device_name = "";

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
	printf(" -v, --visual               : Visual mode - paint the gesture on screen.\n");
	printf(" -h, --help                 : Help\n");
}

static void process_arguments(int argc, char *const *argv)
{

	char opt;
	static struct option opts[] = {
		{"device", required_argument, 0, 'd'},
		{"fingers", required_argument, 0, 'f'},
		{"config", required_argument, 0, 'c'},
		{"help", no_argument, 0, 'h'},
		{"visual", no_argument, 0, 'v'},
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
			nfingers = atoi(optarg);
			break;

		case 'd':
			device_name = strdup(optarg);
			break;

		case 'v':
			visual = 1;
			break;

		case 'c':
			config_file = strdup(optarg);
			break;

		case 'l':
			list_devices_flag = 1;
			break;

		case 'h':
			touchgestures_usage();
			exit(0);
			break;
		}
	}

	if (optind < argc)
	{
		config_file = argv[optind++];
	}

	if (optind < argc)
	{
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		putchar('\n');
	}
}

int main(int argc, char *const *argv)
{

	process_arguments(argc, argv);

	Mygestures *mygestures = mygestures_new();

	if (visual)
	{
		mygestures_set_brush_color(mygestures, "red");
	}

	mygestures_load_configuration(mygestures, config_file);

	LibinputGrabber *libinput;

	device_name = "";

	libinput = grabber_libinput_new(device_name, nfingers);

	if (list_devices_flag)
	{
		//printf("not implemented!\n");
		grabber_libinput_list_devices();
		//grabber_libinput_list_devices(libinput);
		exit(0);
	}

	grabber_libinput_loop(libinput, mygestures);

	exit(0);
}
