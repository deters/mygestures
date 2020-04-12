#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#include <getopt.h>
#include <unistd.h>

#include "assert.h"

#include "grabbing-xinput.h"

enum
{
	IT_DEFAULT,
	IT_XINPUT,
	IT_SYNAPTICS_SHM,
	IT_LIBINPUT
} INPUT_TYPE;

int input_type = 1;
int trigger_button = 3;
char *device_name = "";

char *brush_color;
int list_devices_flag;
char *custom_config_file;

static void mygestures_usage()
{
	printf("Usage: mygestures [OPTIONS] [CONFIG_FILE]\n");
	printf("\n");
	//printf("CONFIG_FILE:\n");

	//char *default_file = mygestures. configuration_get_default_filename(mygestures);
	//printf(" Default: %s\n", default_file);
	//free(default_file);

	printf("\n");
	printf("OPTIONS:\n");
	printf(" -d, --device <DEVICENAME>  : Device to grab\n");
	printf(" -b, --button <BUTTON>      : Button used to draw the gesture\n");
	printf("                              Default: '1' on touchscreens,\n");
	printf("                                       '3' on other pointer dev\n");
	printf(" -l, --device-list          : Print all available devices an exit.\n");
	printf(" -v, --visual               : Don't paint the gesture on screen.\n");
	printf(" -c, --color                : Brush color.\n");
	printf("                              Default: blue\n");
	printf("                              Options: yellow, white, red, green, purple, blue\n");
	printf(" -h, --help                 : Help\n");
	printf(" -i, --inputmode           :  x = xinput, i = liblinput, s=synaptics_shm \n");
	printf("                              It depends on this patched synaptics driver to work:\n");
	printf("                               https://github.com/Chosko/xserver-xorg-input-synaptics\n");
}

static void process_arguments(int argc, char *const *argv)
{

	char opt;
	static struct option opts[] = {
		{"device", required_argument, 0, 'd'},
		{"button", required_argument, 0, 'b'},
		{"color", required_argument, 0, 'c'},
		{"help", no_argument, 0, 'h'},
		{"visual", no_argument, 0, 'v'},
		{"inputmode", required_argument, 0, 'i'},
		{0, 0, 0, 0}};

	/* read params */

	while (1)
	{
		opt = getopt_long(argc, argv, "b:c:d:vhli:", opts, NULL);
		if (opt == -1)
			break;

		switch (opt)
		{

		case 'b':
			trigger_button = atoi(optarg);
			break;

		case 'd':
			device_name = strdup(optarg);
			break;

		case 'i':

			input_type = IT_XINPUT;

			if (strcmp(optarg, "i") == 0)
			{
				input_type = IT_LIBINPUT;
			}

			if (strcmp(optarg, "s") == 0)
			{
				input_type = IT_SYNAPTICS_SHM;
			}

			break;

		case 'v':
			if (!(brush_color))
			{
				brush_color = "blue";
			}
			break;

		case 'c':
			brush_color = strdup(optarg);
			break;

		case 'l':
			list_devices_flag = 1;
			break;

		case 'h':
			mygestures_usage();
			exit(0);
			break;
		}
	}

	if (optind < argc)
	{
		custom_config_file = argv[optind++];
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

	if (brush_color)
	{
		mygestures_set_brush_color(mygestures, brush_color);
	}

	XInputGrabber *grabber;

	switch (input_type)
	{
	case IT_XINPUT:

		grabber = grabber_xinput_new(device_name, trigger_button);

		if (list_devices_flag)
		{
			grabber_list_devices(grabber);
			exit(0);
		}

		mygestures_load_configuration(mygestures);

		grabber_xinput_loop(grabber, mygestures);
		printf("Grabbing loop finished for device '%s'.\n", device_name);
		break;

	case IT_LIBINPUT:

		break;

	case IT_SYNAPTICS_SHM:

		break;

	default:
		break;
	}

	exit(0);
}
