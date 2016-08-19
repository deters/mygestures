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
#include <unistd.h>
#include <wait.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/types.h>

#include "assert.h"
#include "config.h"

#include "grabbing.h"
#include "configuration.h"
#include "configuration_parser.h"

struct shm_message * message;

typedef struct parameters_ {
	int help;
	int button;
	int without_brush;
	int run_as_daemon;
	int list_devices;
	int reconfigure;
	int verbose;
	int debug;
	char * custom_config_file;
	char * device;
	char * brush_color;

	Configuration * gestures_configuration;

} Parameters;

struct shm_message {
	int pid;
	int kill;
	int reload;
};

static void mygestures_usage() {
	printf("%s\n\n", PACKAGE_STRING);
	printf("Usage: mygestures [OPTIONS] [CONFIG_FILE]\n");
	printf("\n");
	printf("CONFIG_FILE:\n");

	char * default_file = xml_get_default_filename();
	printf(" Default: %s\n", default_file);
	free(default_file);

	printf("\n");
	printf("OPTIONS:\n");
	printf(" -b, --button <BUTTON>      : Button used to draw the gesture\n");
	printf("                              Default: '1' on touchscreen devices,\n");
	printf("                                       '3' on other pointer devices\n");
	printf(" -d, --device <DEVICENAME>  : Device to grab.\n");
	printf("                              Default: 'Virtual core pointer'\n");
	printf(" -l, --device-list          : Print all available devices an exit.\n");
	printf(" -z, --daemonize            : Fork the process and return.\n");
	printf(" -c, --brush-color          : Brush color.\n");
	printf("                              Default: blue\n");
	printf("                              Options: yellow, white, red, green, purple, blue\n");
	printf(" -w, --without-brush        : Don't paint the gesture on screen.\n");
	printf(" -v, --verbose              : Increase the verbosity\n");
	printf(" -h, --help                 : Help\n");
}

char * char_replace(char *str, char oldChar, char newChar) {
	assert(str);
	char *strPtr = str;
	while ((strPtr = strchr(strPtr, oldChar)) != NULL)
		*strPtr++ = newChar;
	return str;
}

char* get_shm_name(char* device_name) {

	// the unique_identifier = mygestures + uid + device being grabbed

	char* shm_identifier;
	if (device_name) {
		int bytes = asprintf(&shm_identifier, "/mygestures_uid_%d_dev_%s", getuid(),
				char_replace(device_name, '/', '%'));
	} else {
		int bytes = asprintf(&shm_identifier, "/mygestures_uid_%d_dev_%s", getuid(),
				"DEFAULT_DEVICE");
	}
	return shm_identifier;
}

/*
 * Ask other instances with same unique_identifier to exit.
 */
static void send_kill_message(char * device_name) {

	assert(message);

	/* if shared message contains a PID, kill that process */
	if (message->pid > 0) {
		fprintf(stdout, "\nAsking mygestures running on pid %d to exit. '%s'.\n\n", message->pid,
				get_shm_name(device_name));

		int running = message->pid;

		message->pid = getpid();
		message->kill = 1;

		int err = kill(running, SIGINT);

		/* give some time. ignore failing */
		usleep(100 * 1000); // 100ms

	}

	/* write own PID in shared memory */
	message->pid = getpid();
	message->kill = 0;

}

static void send_reload_message() {

	/* if shared message contains a PID, kill that process */
	if (message->pid > 0) {
		fprintf(stdout, "\nAsking mygestures running on pid %d to reload.\n", message->pid);

		int running = message->pid;

		message->pid = getpid();
		message->reload = 1;

		int err = kill(running, SIGINT);

	}

}

static void alloc_shared_memory(char * device_name) {

	char* shm_identifier = get_shm_name(device_name);

	int shared_seg_size = sizeof(struct shm_message);
	int shmfd = shm_open(shm_identifier, O_CREAT | O_RDWR, 0600);

	free(shm_identifier);

	if (shmfd < 0) {
		perror("In shm_open()");
		exit(shmfd);
	}
	int err = ftruncate(shmfd, shared_seg_size);

	message = (struct shm_message *) mmap(NULL, shared_seg_size,

	PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
	if (message == NULL) {
		perror("In mmap()");
		exit(1);
	}

}

//static void release_shared_memory() {
//
//	/*  If your head comes away from your neck, it's over! */
//
//	if (shm_identifier) {
//
//		if (shm_unlink(shm_identifier) != 0) {
//			perror("In shm_unlink()");
//			exit(1);
//		}
//
//		free(shm_identifier);
//
//	}
//}

static void on_kill(int a) {
	//release_shared_memory();
	exit(0);
}

static void on_interrupt(int a) {

	if (message->kill) {
		printf("\nMygestures on PID %d asked me to exit.\n", message->pid);
		// shared memory now belongs to the other process. will not be released
	} else if (message->reload) {
		printf("\nMygestures on PID %d asked me to reload.\n", message->pid);
		message->pid = getpid();
		message->reload = 0;
		return;
	} else {
		printf("\nReceived the interrupt signal.\n");
		//release_shared_memory();

	}

	exit(0);
}

static void daemonize() {
	int i;

	i = fork();
	if (i != 0)
		exit(0);

	i = chdir("/");
	return;
}

Parameters * mygestures_new() {
	Parameters *self = malloc(sizeof(Parameters));
	bzero(self, sizeof(Parameters));
	return self;
}

void mygestures_load_configuration(Parameters * self) {

	if (self->custom_config_file) {
		self->gestures_configuration = xml_load_engine_from_file(self->custom_config_file);
	} else {
		self->gestures_configuration = xmlconfig_load_engine_from_defaults();
	}

}

void mygestures_parse_arguments(Parameters * self, int argc, char * const *argv) {

	char opt;
	static struct option opts[] = {
			{
					"verbose", no_argument, 0, 'v' }, {
					"help", no_argument, 0, 'h' }, {
					"without-brush", no_argument, 0, 'w' }, {
					"daemonize", no_argument, 0, 'z' }, {
					"button", required_argument, 0, 'b' }, {
					"brush-color", required_argument, 0, 'b' }, {
					"device",
					required_argument, 0, 'd' }, {
					0, 0, 0, 0 } };

	/* read params */

	while (1) {
		opt = getopt_long(argc, argv, "b:c:d:vhlwx:zr", opts, NULL);
		if (opt == -1)
			break;

		switch (opt) {

		case 'd':
			self->device = optarg;
			break;

		case 'b':
			self->button = atoi(optarg);
			break;

		case 'c':
			self->brush_color = optarg;
			break;

		case 'w':
			self->without_brush = 1;
			break;

		case 'l':
			self->list_devices = 1;
			break;

		case 'z':
			self->run_as_daemon = 1;
			break;

		case 'v':
			self->verbose = 1;
			break;

		case 'h':
			self->help = 1;
			break;

		}

	}

	if (optind < argc) {
		self->custom_config_file = argv[optind++];
	}

	if (optind < argc) {
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		putchar('\n');

	}

	/* apply params */

	if (self->run_as_daemon)
		daemonize();

	if (self->help) {
		mygestures_usage();
		exit(0);
	}

	mygestures_load_configuration(self);

}

void mygestures_run(Parameters * self) {

	int device_count = 2;

	char * default_devices[2] = {
			"Virtual Core Pointer", "synaptics" };

	if (self->device) {
		device_count = 1;
		default_devices[0] = self->device;
	}

	for (int i = 0; i < device_count; ++i) {

		char * device_name = default_devices[i];

		int in_fork = fork();

		if (in_fork) {

			alloc_shared_memory(device_name);

			if (self->reconfigure) {
				send_reload_message();
				exit(0);
			} else {
				send_kill_message(device_name);
			}

			signal(SIGINT, on_interrupt);
			signal(SIGKILL, on_kill);

			Grabber * grabber = grabber_init(device_name, self->button, self->without_brush,
					self->list_devices, self->brush_color, self->verbose);

			printf("Grabbing device %s.\n", device_name);

			grabber_loop(grabber, self->gestures_configuration);
			//grabber_finalize(grabber);

		} else {

		}

	}

}

int main(int argc, char * const * argv) {

	Parameters *self = mygestures_new();

	mygestures_parse_arguments(self, argc, argv);

	mygestures_run(self);

	exit(0);

}
