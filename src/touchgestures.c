#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#include <getopt.h>
#include <unistd.h>

#include "assert.h"

#include "libinput-grabber.h"

int touch_nfingers = 3;

static void touchgestures_usage()
{
	printf("Usage: touchgestures [OPTIONS]\n");
	printf("\n");
	//printf("CONFIG_FILE:\n");

	//char *default_file = mygestures. configuration_get_default_filename(mygestures);
	//printf(" Default: %s\n", default_file);
	//free(default_file);

	printf("\n");
	printf("TOUCHGESTURES OPTIONS:\n");
	printf(" -f, --fingers <FINGERS>    : How many fingers will trigger the gesture. Default: 3\n");
}

static void touchgestures_process_arguments(int argc, char *const *argv)
{

	char opt;
	static struct option opts[] = {
		{"fingers", required_argument, 0, 'f'},
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
		}
	}
}

int touchgestures_main(int argc, char *const *argv, Mygestures *mygestures)
{

	touchgestures_process_arguments(argc, argv);

	LibinputGrabber *libinput;

	libinput = libinput_grabber_new("", touch_nfingers);

	libinput_grabber_loop(libinput, mygestures);

	exit(0);
}
