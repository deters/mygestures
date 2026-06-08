#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include "mygestures.h"
#include "ipc.h"

static void process_arguments(Mygestures *self, int argc, char *const *argv)
{

	int opt;
	static struct option opts[] = {
		{"device", required_argument, 0, 'd'},
		{"button", required_argument, 0, 'b'},
		{"sensitivity", required_argument, 0, 's'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}};

	/* read params */

	while (1)
	{
		opt = getopt_long(argc, argv, "b:d:h", opts, NULL);
		if (opt == -1)
			break;

		switch (opt)
		{

		case 'b':
			self->trigger_button = atoi(optarg);
			break;

		case 's':
			self->sensitivity = atoi(optarg);
			break;

		case 'd':
			if (self->device_count < MAX_GRABBED_DEVICES) {
				self->device_list[self->device_count++] = strdup(optarg);
			} else {
				fprintf(stderr, "Warning: Maximum devices (%d) reached. Ignoring device '%s'.\n", MAX_GRABBED_DEVICES, optarg);
			}
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
	if (!self) {
		return 1;
	}

	process_arguments(self, argc, argv);

	mygestures_run(self);

	return 0;
}
