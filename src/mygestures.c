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

#include <fcntl.h>
#include <signal.h>

uint MAX_GRABBED_DEVICES = 10;

#include <sys/mman.h>
#include <sys/shm.h>

struct shm_message
{
	int pid;
	int kill;
};

static struct shm_message *message;
static char *shm_identifier;

/*
 * Ask other instances with same unique_identifier to exit.
 */
void send_kill_message(char *device_name)
{

	assert(message);

	/* if shared message contains a PID, kill that process */
	if (message->pid > 0)
	{
		printf("Asking mygestures running on pid %d to exit..\n", message->pid);

		int running = message->pid;

		message->pid = getpid();
		message->kill = 1;

		int err = kill(running, SIGINT);

		if (err)
		{
			printf("Error sending kill message.\n");
		}

		/* give some time. ignore failing */
		usleep(100 * 1000); // 100ms
	}

	/* write own PID in shared memory */
	message->pid = getpid();
	message->kill = 0;
}

static void char_replace(char *str, char oldChar, char newChar)
{
	assert(str);
	char *strPtr = str;
	while ((strPtr = strchr(strPtr, oldChar)) != NULL)
		*strPtr++ = newChar;
}

void alloc_shared_memory(char *device_name, int button)
{

	char *sanitized_device_name = strdup(device_name);

	if (sanitized_device_name)
	{
		char_replace(sanitized_device_name, '/', '%');
	}
	else
	{
		sanitized_device_name = "";
	}

	asprintf(&shm_identifier, "/mygestures_uid_%d_dev_%s_button_%d", getuid(),
			 sanitized_device_name, button);

	int shared_seg_size = sizeof(struct shm_message);
	int shmfd = shm_open(shm_identifier, O_CREAT | O_RDWR, 0600);

	//free(shm_identifier);

	if (shmfd < 0)
	{
		perror("In shm_open()");
		exit(shmfd);
	}
	int err = ftruncate(shmfd, shared_seg_size);

	if (err)
	{
		printf("Error truncating SHM variable\n");
	}

	message = (struct shm_message *)mmap(NULL, shared_seg_size,
										 PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

	if (message == NULL)
	{
		perror("In mmap()");
		exit(1);
	}
}

static void release_shared_memory()
{

	/*  If your head comes away from your neck, it's over! */

	if (shm_identifier)
	{

		if (shm_unlink(shm_identifier) != 0)
		{
			perror("In shm_unlink()");
			exit(1);
		}

		free(shm_identifier);
	}
}

void on_interrupt(int a)
{

	if (message->kill)
	{
		printf("\nMygestures on PID %d asked me to exit.\n", message->pid);
		// shared memory now belongs to the other process. will not be released
	}
	else
	{
		printf("\nReceived the interrupt signal.\n");
		release_shared_memory();
	}

	exit(0);
}

void on_kill(int a)
{
	//release_shared_memory();

	exit(0);
}

Mygestures *mygestures_new()
{

	Mygestures *self = malloc(sizeof(Mygestures));
	bzero(self, sizeof(Mygestures));

	self->device_list = malloc(sizeof(uint) * MAX_GRABBED_DEVICES);
	self->gestures_configuration = configuration_new();

	return self;
}

static void mygestures_load_configuration(Mygestures *self)
{

	if (self->custom_config_file)
	{
		configuration_load_from_file(self->gestures_configuration,
									 self->custom_config_file);
	}
	else
	{
		configuration_load_from_defaults(self->gestures_configuration);
	}
}

static void mygestures_grab_device(Mygestures *self, char *device_name)
{

	int p = fork();

	if (p != 0)
	{

		/* We are in the forked thread. Start grabbing a device */

		printf("Listening to device '%s'\n\n", device_name);

		alloc_shared_memory(device_name, self->trigger_button);

		Grabber *grabber = grabber_new(device_name, self->trigger_button);

		grabber_set_brush_color(grabber, self->brush_color);

		send_kill_message(device_name);

		signal(SIGINT, on_interrupt);
		signal(SIGKILL, on_kill);

		if (self->list_devices_flag)
		{
			grabber_list_devices(grabber);
		}
		else
		{
			grabber_loop(grabber, self->gestures_configuration);
		}
	}
}

void mygestures_run(Mygestures *self)
{

	printf("%s\n\n", PACKAGE_STRING);
	/*
	 * Will not load configuration if it is only listing the devices.
	 */
	if (!self->list_devices_flag)
	{
		mygestures_load_configuration(self);
	}

	if (self->libinput)
	{
	}
	else
	{

		if (self->multitouch)
		{
			printf("Starting in multitouch mode.\n");
			mygestures_grab_device(self, "synaptics");
		}
		else
		{

			if (self->device_count)
			{
				/*
		 * Start grabbing any device passed via argument flags.
		 */
				for (int i = 0; i < self->device_count; ++i)
				{
					mygestures_grab_device(self, self->device_list[i]);
				}
			}
			else
			{

				printf("Selecting default xinput device.\n");
				mygestures_grab_device(self, "Virtual Core Pointer");
				/*
		 * If there where no devices in the argument flags, then grab the default devices.
		 */
			}
		}
	}
}
