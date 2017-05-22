/*
 Copyright 2013-2016 Lucas Augusto Deters
 Copyright 2005 Nir Tzachar

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 one line to give the program's name and an idea of what it does.
 */

#define _GNU_SOURCE /* needed by asprintf */
#include <stdio.h>
#include <stdlib.h>

#include <wait.h>
#include <unistd.h>
#include <signal.h>

#include <sys/types.h>

#include "assert.h"
#include "string.h"
#include "config.h"

#include "mygestures.h"
#include "main.h"

#include "grabbing.h"
#include "configuration.h"
#include "configuration_parser.h"

uint MAX_GRABBED_DEVICES = 10;

static
void mygestures_usage(Mygestures * self) {
	printf("Usage: mygestures [OPTIONS] [CONFIG_FILE]\n");
	printf("\n");
	printf("CONFIG_FILE:\n");

	char * default_file = configuration_get_default_filename(self);
	printf(" Default: %s\n", default_file);
	free(default_file);

	printf("\n");
	printf("OPTIONS:\n");
	printf(" -b, --button <BUTTON>      : Button used to draw the gesture\n");
	printf("                              Default: '1' on touchscreens,\n");
	printf("                                       '3' on other pointer dev\n");
	printf(" -d, --device <DEVICENAME>  : Device to grab.\n");
	printf(
			"                              Defaults: 'Virtual core pointer' & 'synaptics'\n");
	printf(
			" -l, --device-list          : Print all available devices an exit.\n");
	printf(" -z,  --daemonize            : Fork the process and return.\n");
	printf(" -c,  --color                : Brush color.\n");
	printf("                               Default: blue\n");
	printf(
			"                              Options: yellow, white, red, green, purple, blue\n");
	printf(
			" -w, --without-brush        : Don't paint the gesture on screen.\n");
	printf(" -v, --verbose              : Increase the verbosity\n");
	printf(" -h, --help                 : Help\n");
}

Mygestures * mygestures_new() {

	Mygestures *self = malloc(sizeof(Mygestures));
	bzero(self, sizeof(Mygestures));

	self->brush_color = "blue";
	self->trigger_button = 0;
	self->custom_config_file = NULL;
	self->device_count = 0;
	self->device_list = malloc(sizeof(uint) * MAX_GRABBED_DEVICES);
	self->gestures_configuration = NULL;
	self->help_flag = 0;
	self->list_devices_flag = 0;
	self->damonize_option = 0;
	self->verbose_option = 0;
	self->without_brush_option = 0;
	self->gestures_configuration = configuration_new();

	return self;
}

static
void mygestures_load_configuration(Mygestures * self) {

	if (self->custom_config_file) {
		configuration_load_from_file(self->gestures_configuration,
				self->custom_config_file);
	} else {
		configuration_load_from_defaults(self->gestures_configuration);
	}

}

static
void mygestures_grab_device(Mygestures* self, char* device_name) {

	int p = fork();

	if (p == 0) {

		/* we are in the forked thread. start grabbing a device */

		printf("Listening device %s\n", device_name);

		alloc_shared_memory(device_name);




		Grabber* grabber = grabber_init(device_name, self->trigger_button,
				self->without_brush_option,
				self->brush_color, self->verbose_option);

		send_kill_message(device_name);

		signal(SIGINT, on_interrupt);
		signal(SIGKILL, on_kill);

		grabber_loop(grabber, self->gestures_configuration);
	}

}

void mygestures_run(Mygestures * self) {

	printf("%s\n\n", PACKAGE_STRING);

	if (self->help_flag) {
		mygestures_usage(self);
		exit(0);
	}

	/*
	 * Only load configurations if the device list is empty
	 */
	if (!self->list_devices_flag) {
		mygestures_load_configuration(self);
	}

	/*
	 * Start grabbing any device passed via argument flags.
	 */
	for (int i = 0; i < self->device_count; ++i) {
		mygestures_grab_device(self, self->device_list[i]);
	}

	/*
	 * If there where no devices in the argument flags, then grab the default devices.
	 */

	if (self->device_count == 0) {
		mygestures_grab_device(self, "Virtual Core Pointer");
		mygestures_grab_device(self, "synaptics");
	}

}

