#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include "assert.h"

#include <getopt.h>

#include "mousegestures.h"
#include "touchgestures.h"

#include "gestures.h"

int list_devices_flag;
char *config_file;
char *device_name = "";

static void mouse_usage()
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

		case 'c':
			config_file = strdup(optarg);
			break;

		case 'l':
			list_devices_flag = 1;
			break;

		case 'h':
			mouse_usage();
			exit(0);
			break;

		case 'd':
			device_name = strdup(optarg);
			break;
		}
	}
}

int main(int argc, char *const *argv)
{

	process_arguments(argc, argv);

	Mygestures *mygestures = mygestures_new();
	mygestures_load_configuration(mygestures, config_file);

	int pid = fork();

	if (pid == 0)
	{
		touchgestures_main(argc, argv, mygestures);
	}
	else
	{
		mousegestures_main(argc, argv, mygestures);
	}

	exit(0);
}
