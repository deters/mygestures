#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "assert.h"

#include "mygestures.h"
#include "ipc.h"

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
		{"evdev", no_argument, 0, 'e'},
		{"create-config", no_argument, 0, 'C'},
		{0, 0, 0, 0}};

	/* read params */

	while (1)
	{
		opt = getopt_long(argc, argv, "b:c:d:vhlmeC", opts, NULL);
		if (opt == -1)
			break;

		switch (opt)
		{

		case 'C':
			self->create_config_flag = 1;
			break;

		case 'b':
			self->trigger_button = atoi(optarg);
			break;

		case 'd':
			if (self->device_count < MAX_GRABBED_DEVICES) {
				self->device_list[self->device_count++] = strdup(optarg);
			} else {
				fprintf(stderr, "Warning: Maximum devices (%d) reached. Ignoring device '%s'.\n", MAX_GRABBED_DEVICES, optarg);
			}
			break;

		case 'm':
			self->multitouch = 1;
			break;

		case 'e':
			self->evdev = 1;
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
			self->help_flag = 1;
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
