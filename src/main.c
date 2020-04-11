#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#include <getopt.h>
#include <unistd.h>

#include "assert.h"

#include "mygestures.h"

static void mygestures_usage()
{
	printf("Usage: mygestures [OPTIONS] [CONFIG_FILE]\n");
	printf("\n");
	//printf("CONFIG_FILE:\n");

	//char *default_file = self. configuration_get_default_filename(self);
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
	printf(" -m, --multitouch           : Multitouch mode on some synaptic touchpads.\n");
	printf("                              It depends on this patched synaptics driver to work:\n");
	printf("                               https://github.com/Chosko/xserver-xorg-input-synaptics\n");
}

static void process_arguments(Mygestures *self, int argc, char *const *argv)
{

	char opt;
	static struct option opts[] = {
		{"device", required_argument, 0, 'd'},
		{"button", required_argument, 0, 'b'},
		{"color", required_argument, 0, 'c'},
		{"help", no_argument, 0, 'h'},
		{"visual", no_argument, 0, 'v'},
		{"multitouch", no_argument, 0, 'm'},
		{0, 0, 0, 0}};

	/* read params */

	while (1)
	{
		opt = getopt_long(argc, argv, "b:c:d:vhlm", opts, NULL);
		if (opt == -1)
			break;

		switch (opt)
		{

		case 'b':
			self->trigger_button = atoi(optarg);
			break;

		case 'd':
			self->device_list[self->device_count++] = strdup(optarg);
			break;

		case 'm':
			self->multitouch = 1;
			break;

		case 'v':
			if (!(self->brush_color))
			{
				self->brush_color = "blue";
			}
			break;

		case 'c':
			self->brush_color = strdup(optarg);
			break;

		case 'l':
			self->list_devices_flag = 1;
			break;

		case 'h':
			mygestures_usage();
			exit(0);
			break;
		}
	}

	if (optind < argc)
	{
		self->custom_config_file = argv[optind++];
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

	Mygestures *self = mygestures_new();

	process_arguments(self, argc, argv);

	mygestures_run(self);

	exit(0);
}
