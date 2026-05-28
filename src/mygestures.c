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
#include <fcntl.h>
#include <errno.h>

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
#include "grabbing-evdev.h"
#include "configuration.h"
#include "configuration_parser.h"

uint MAX_GRABBED_DEVICES = 10;

static void mygestures_usage(Mygestures *self)
{
	printf("Usage: mygestures [OPTIONS] [CONFIG_FILE]\n");
	printf("\n");
	printf("CONFIG_FILE:\n");

	char *default_file = configuration_get_default_filename();
	printf(" Default: %s\n", default_file);
	free(default_file);

	printf("\n");
	printf("OPTIONS:\n");
	printf(" -d, --device <DEVICENAME>  : Device to grab\n");
	printf(" -b, --button <BUTTON>      : Button used to draw the gesture\n");
	printf("                              Default: '1' on touchscreens,\n");
	printf("                                       '3' on pointer devices\n");
	printf(" -l, --device-list          : Print all available devices an exit.\n");
	printf(" -C, --create-config        : Create a default config file in the user's config directory.\n");
	printf(" -v, --visual               : Don't paint the gesture on screen.\n");
	printf(" -c, --color                : Brush color.\n");
	printf("                              Default: blue\n");
	printf("                              Options: yellow, white, red, green, purple, blue\n");
	printf(" -h, --help                 : Help\n");
	printf(" -m, --multitouch           : Multitouch mode on some synaptic touchpads.\n");
	printf("                              It depends on this patched synaptics driver to work:\n");
	printf("                               https://github.com/Chosko/xserver-xorg-input-synaptics\n");
	printf(" -e, --evdev                : Read events directly using libevdev (requires sudo).\n");
}

static int check_permissions_and_guide(Mygestures *self)
{
	int uinput_ok = 0;
	int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (fd >= 0) {
		uinput_ok = 1;
		close(fd);
	} else {
		fd = open("/dev/misc/uinput", O_WRONLY | O_NONBLOCK);
		if (fd >= 0) {
			uinput_ok = 1;
			close(fd);
		}
	}

	int devices_ok = 1;
	if (self->device_count) {
		for (int i = 0; i < self->device_count; ++i) {
			int dev_fd = open(self->device_list[i], O_RDONLY | O_NONBLOCK);
			if (dev_fd >= 0) {
				close(dev_fd);
			} else {
				devices_ok = 0;
				break;
			}
		}
	} else {
		char device_path[256];
		if (find_mouse_device(device_path, sizeof(device_path)) == 0) {
			int dev_fd = open(device_path, O_RDONLY | O_NONBLOCK);
			if (dev_fd >= 0) {
				close(dev_fd);
			} else {
				devices_ok = 0;
			}
		} else {
			devices_ok = 0;
		}
	}

	if (!uinput_ok || !devices_ok) {
		fprintf(stderr, "\n=========================================================================\n");
		fprintf(stderr, "ERROR: Missing permissions to run mygestures in evdev mode.\n");
		if (!devices_ok) {
			fprintf(stderr, " - Cannot read mouse input device(s).\n");
		}
		if (!uinput_ok) {
			fprintf(stderr, " - Cannot write to /dev/uinput virtual device creator.\n");
		}
		fprintf(stderr, "\nTo resolve this without running as root (via sudo), perform the following:\n\n");
		fprintf(stderr, "1. Add your user to the 'input' group:\n");
		fprintf(stderr, "   sudo usermod -aG input $USER\n");
		fprintf(stderr, "   (Note: You will need to log out and log back in for this to take effect)\n\n");
		fprintf(stderr, "2. Ensure the mygestures udev rules are installed to allow non-root uinput device creation:\n");
		fprintf(stderr, "   - If you installed via package (e.g. deb), the rules are already installed.\n");
		fprintf(stderr, "   - If you built from source, copy the rules file manually:\n");
		fprintf(stderr, "     sudo cp 99-mygestures.rules /etc/udev/rules.d/\n");
		fprintf(stderr, "     sudo udevadm control --reload-rules && sudo udevadm trigger\n");
		fprintf(stderr, "=========================================================================\n\n");
		return 0;
	}
	return 1;
}


Mygestures *mygestures_new()
{

	Mygestures *self = malloc(sizeof(Mygestures));
	bzero(self, sizeof(Mygestures));

	self->device_list = malloc(sizeof(char *) * MAX_GRABBED_DEVICES);
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
		configuration_load_from_defaults(self->gestures_configuration, self->create_config_flag);
	}
}

static void mygestures_grab_device(Mygestures *self, char *device_name)
{

	int p = fork();

	if (p == 0)
	{

		/* We are in the child process. Start grabbing a device */

		printf("Listening to device '%s'\n\n", device_name);

		int trigger_button = self->trigger_button;
		if (trigger_button == 0)
		{
			const char *desktop = getenv("XDG_CURRENT_DESKTOP");
			int is_gnome = 0;
			if (desktop && (strstr(desktop, "GNOME") != NULL || strstr(desktop, "gnome") != NULL))
			{
				is_gnome = 1;
			}

			if (is_gnome)
			{
				trigger_button = 3;
			}
			else if (self->multitouch || (device_name && strcasecmp(device_name, "synaptics") == 0))
			{
				trigger_button = 1;
			}
			else
			{
				trigger_button = 3;
			}
		}

		alloc_shared_memory(device_name, trigger_button);

		Grabber *grabber = grabber_new(device_name, trigger_button);
		grabber->evdev = self->evdev;

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
		exit(0);
	}
}

void mygestures_run(Mygestures *self)
{

	printf("%s\n\n", PACKAGE_STRING);

	if (self->help_flag)
	{
		mygestures_usage(self);
		exit(0);
	}

	/* Select capture method based on the environment */
	if (!self->evdev && !self->multitouch)
	{
		const char *wayland_display = getenv("WAYLAND_DISPLAY");
		const char *xdg_session = getenv("XDG_SESSION_TYPE");
		int is_wayland = (wayland_display != NULL) || (xdg_session && strcmp(xdg_session, "wayland") == 0);

		const char *display_env = getenv("DISPLAY");
		int has_x11 = (display_env != NULL && strlen(display_env) > 0);

		if (is_wayland || !has_x11)
		{
			printf("Wayland or no X11 display environment detected. Automatically enabling evdev capture mode.\n");
			if (getuid() != 0)
			{
				printf("Running as non-root user. If device opening fails, ensure your user has input/uinput permissions.\n");
			}
			self->evdev = 1;
		}
		else
		{
			self->evdev = 0;
		}
	}

	/* Resolve default trigger button if not specified */
	if (self->trigger_button == 0)
	{
		const char *desktop = getenv("XDG_CURRENT_DESKTOP");
		int is_gnome = 0;
		if (desktop && (strstr(desktop, "GNOME") != NULL || strstr(desktop, "gnome") != NULL))
		{
			is_gnome = 1;
		}

		if (is_gnome)
		{
			self->trigger_button = 3;
		}
		else
		{
			int is_synaptics = self->multitouch;
			if (!is_synaptics && self->device_count > 0)
			{
				for (int i = 0; i < self->device_count; i++)
				{
					if (self->device_list[i] && strcasecmp(self->device_list[i], "synaptics") == 0)
					{
						is_synaptics = 1;
						break;
					}
				}
			}

			if (is_synaptics)
			{
				self->trigger_button = 1;
			}
			else
			{
				self->trigger_button = 3;
			}
		}
	}

	/* Always print the trigger button when starting the daemon */
	if (!self->list_devices_flag)
	{
		printf("Trigger button: %d\n", self->trigger_button);
		if (self->multitouch)
		{
			printf("Capture method: multitouch\n");
		}
		else if (self->evdev)
		{
			printf("Capture method: evdev\n");
		}
		else
		{
			printf("Capture method: XInput\n");
		}
	}

	/*
	 * Will not load configuration if it is only listing the devices.
	 */
	if (!self->list_devices_flag)
	{
		mygestures_load_configuration(self);
	}

	if (self->multitouch)
	{
		printf("Starting in multitouch mode.\n");
		mygestures_grab_device(self, "synaptics");
	}
	else if (self->evdev)
	{
		if (!check_permissions_and_guide(self))
		{
			exit(1);
		}
		printf("Starting in evdev mode.\n");
		if (self->device_count)
		{
			for (int i = 0; i < self->device_count; ++i)
			{
				mygestures_grab_device(self, self->device_list[i]);
			}
		}
		else
		{
			char device_path[256];
			if (find_mouse_device(device_path, sizeof(device_path)) == 0)
			{
				mygestures_grab_device(self, device_path);
			}
			else
			{
				fprintf(stderr, "Could not find default evdev mouse device.\n");
				exit(1);
			}
		}
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

	/* Wait for all child grabber processes to finish */
	int status;
	while (wait(&status) > 0);
}
