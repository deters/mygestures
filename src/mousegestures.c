#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#include <getopt.h>
#include <unistd.h>

#include "assert.h"

#include "xinput-grabber.h"

int trigger_button = 3;

static void mouse_usage()
{
	printf("\n");
	printf("MOUSE GESTURE OPTIONS:\n");
	printf(" -b, --button <BUTTON>      : Button to grab.\n");
}

static void process_arguments(int argc, char *const *argv)
{

	char opt;
	static struct option opts[] = {
		{"button", required_argument, 0, 'b'},
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
			trigger_button = atoi(optarg);
			break;
		}
	}
}

int mousegestures_main(int argc, char *const *argv, Mygestures *mygestures)
{

	process_arguments(argc, argv);

	XInputGrabber *xinput;

	xinput = grabber_xinput_new("", trigger_button);

	grabber_xinput_loop(xinput, mygestures);

	exit(0);
}
