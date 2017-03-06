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

	char * default_file = xml_get_default_filename(self);
	printf(" Default: %s\n", default_file);
	free(default_file);

	printf("\n");
	printf("OPTIONS:\n");
	printf(" -b, --button <BUTTON>      : Button used to draw the gesture\n");
	printf(
			"                              Default: '1' on touchscreen devices,\n");
	printf(
			"                                       '3' on other pointer devices\n");
	printf(" -d, --device <DEVICENAME>  : Device to grab.\n");
	printf(
			"                              By default try to grab both 'Virtual core pointer' and 'synaptics'\n");
	printf(
			" -l, --device-list          : Print all available devices an exit.\n");
	printf(" -z, --daemonize            : Fork the process and return.\n");
	printf(" -c, --brush-color          : Brush color.\n");
	printf("                              Default: blue\n");
	printf(
			"                              Options: yellow, white, red, green, purple, blue\n");
	printf(
			" -w, --without-brush        : Don't paint the gesture on screen.\n");
	printf(" -v, --verbose              : Increase the verbosity\n");
	printf(" -h, --help                 : Help\n");
}





static Mygestures * mygestures_new() {


	Mygestures *self = malloc(sizeof(Mygestures));
	bzero(self, sizeof(Mygestures));

	self->brush_color = NULL;
	self->button = 0;
	self->custom_config_file = NULL;
	self->device_count = 0;
	self->device_list = malloc(sizeof(uint) * MAX_GRABBED_DEVICES);
	self->gestures_configuration = NULL;
	self->help = 0;
	self->list_devices = 0;
	self->run_as_daemon = 0;
	self->verbose = 0;
	self->without_brush = 0;



	return self;
}

static
void mygestures_load_configuration(Mygestures * self) {

	if (self->custom_config_file) {
		self->gestures_configuration = xml_load_engine_from_file(
				self->custom_config_file);
	} else {
		self->gestures_configuration = xmlconfig_load_engine_from_defaults();
	}

}


static
void mygestures_grab_device(Mygestures* self, char* device_name) {

	int newpid = fork();

	if (newpid) {

		/* Print the new pid in the main thread. */
		printf("%i: Grabbing device '%s'\n", newpid, device_name);

	} else {

		/* Allow */

		alloc_shared_memory(device_name);

		Grabber* grabber = grabber_init(device_name, self->button,
				self->without_brush, self->list_devices, self->brush_color,
				self->verbose);

		if (self->list_devices) {
			grabber_print_devices(grabber);
			exit(0);
		}

		send_kill_message(device_name);

		signal(SIGINT, on_interrupt);
		signal(SIGKILL, on_kill);

//
//		if (self->brush_color){
//			grabber_set_brush_color(grabber, self->brush_color);
//		}

		grabber_loop(grabber, self->gestures_configuration);
		//grabber_finalize(grabber);
	}

}

static
void mygestures_run(Mygestures * self) {

	printf("%s\n\n", PACKAGE_STRING);

	/* apply params */

	if (self->help) {
		mygestures_usage(self);
		exit(0);
	}

	if (!self->list_devices) {
		mygestures_load_configuration(self);
	}


	for (int i = 0; i < self->device_count; ++i) {
		/* Will start a new thread for every device. */
		mygestures_grab_device(self, self->device_list[i]);
	}

	/* Load default devices if needed */
	if (self->device_count == 0) {
		mygestures_grab_device(self, "Virtual Core Pointer");
		mygestures_grab_device(self, "synaptics");
	}

}

int main(int argc, char * const * argv) {

	Mygestures *self = mygestures_new();

	mygestures_read_parameters(self, argc, argv);
	mygestures_run(self);

	exit(0);

}
