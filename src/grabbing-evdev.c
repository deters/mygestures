#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/input-event-codes.h>
#include <poll.h>

#include "grabbing.h"
#include "grabbing-evdev.h"
#include "uinput_device.h"

int find_mouse_device(char *path, size_t len) {
    DIR *dir;
    struct dirent *entry;
    const char *input_dir = "/dev/input/by-path/";
    char fallback[256] = "";

    dir = opendir(input_dir);
    if (!dir) {
        perror("opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Look for entries that indicate a mouse.
        // Favor 'event-mouse' over legacy 'mouse' entries.
        if (strstr(entry->d_name, "event-mouse")) {
            snprintf(path, len, "/dev/input/by-path/%s", entry->d_name);
            closedir(dir);
            return 0;
        }
        if (strstr(entry->d_name, "-mouse") && strlen(fallback) == 0) {
            snprintf(fallback, sizeof(fallback), "/dev/input/by-path/%s", entry->d_name);
        }
    }

    closedir(dir);

    if (strlen(fallback) > 0) {
        strncpy(path, fallback, len);
        return 0;
    }

    fprintf(stderr, "No mouse device found in %s\n", input_dir);
    return -1;
}

static int get_evdev_button_code(int button) {
	switch (button) {
		case 1: return BTN_LEFT;
		case 2: return BTN_MIDDLE;
		case 3: return BTN_RIGHT;
		case 8: return BTN_SIDE;
		case 9: return BTN_EXTRA;
		case 10: return BTN_FORWARD;
		case 11: return BTN_BACK;
		case 12: return BTN_TASK;
		default:
			if (button >= 0x100) return button; // Direct evdev code
			return BTN_RIGHT;
	}
}

void grabber_evdev_loop(Grabber *self, Configuration *conf) {
	struct libevdev *dev = NULL;
	int fd;
	int rc = 1;
	char device_path[256];

	if (self->devicename && (strstr(self->devicename, "/") != NULL)) {
		strncpy(device_path, self->devicename, sizeof(device_path));
		device_path[sizeof(device_path) - 1] = '\0';
	} else {
		if (find_mouse_device(device_path, sizeof(device_path)) != 0) {
			fprintf(stderr, "Failed to find default mouse device.\n");
			return;
		}
	}

	printf("Attempting to open device: %s\n", device_path);
	fd = open(device_path, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", device_path, strerror(errno));
		return;
	}

	rc = libevdev_new_from_fd(fd, &dev);
	if (rc < 0) {
		fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
		close(fd);
		return;
	}

	/* Adjust sensitivity based on device hardware attributes. */
	if (libevdev_has_event_type(dev, EV_ABS)) {
		const struct input_absinfo *abs_x = libevdev_get_abs_info(dev, ABS_X);
		if (abs_x && self->delta_min <= 30) { /* Only if not manually overridden */
			int range = abs_x->maximum - abs_x->minimum;
			int resolution = abs_x->resolution; /* units per mm */

			if (resolution > 0) {
				/* Aim for a threshold of approx 3-5mm of movement */
				self->delta_min = resolution * 4;
				printf("mygestures: High-res device detected (%d units/mm). Setting threshold to %d.\n", 
					   resolution, self->delta_min);
			} else if (range > 0) {
				/* Fallback: use a percentage of the total range (approx 4%) */
				self->delta_min = range / 25;
				printf("mygestures: Absolute device detected (range %d). Setting threshold to %d.\n", 
					   range, self->delta_min);
			}
		}
	} else {
		/* Relative device (Mouse). Standard sensitivity is usually fine. */
		if (self->delta_min <= 0) self->delta_min = 30;
	}

	if (self->button == 0) {
		self->button = 3;
	}

	int grabbed = 0;
	if (self->button == 3) {
		rc = libevdev_grab(dev, LIBEVDEV_GRAB);
		if (rc < 0) {
			fprintf(stderr, "mygestures: Failed to grab device %s exclusively: %s\n",
					libevdev_get_name(dev), strerror(-rc));
		} else {
			printf("mygestures: Grabbed device %s exclusively.\n", libevdev_get_name(dev));
			grabbed = 1;
	if (uinput_init_from_device(dev) < 0) {
		fprintf(stderr, "mygestures: Failed to initialize uinput for forwarding.\n");
	}

	int target_button = get_evdev_button_code(self->button);
	printf("Listening for events from %s using libevdev (button %d)\n", libevdev_get_name(dev), self->button);

	self->is_exclusive = grabbed;

	int moved = 0;
	int virtual_x = 0;
	int virtual_y = 0;

	struct pollfd fds[1];
	fds[0].fd = fd;
	fds[0].events = POLLIN;

	while (!self->shut_down) {
		int ret = poll(fds, 1, 100);
		if (ret < 0) {
			if (errno == EINTR) continue;
			break;
		}
		if (ret == 0) continue; // Timeout

		struct input_event ev;
		while ((rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) == LIBEVDEV_READ_STATUS_SUCCESS ||
			   rc == LIBEVDEV_READ_STATUS_SYNC) {
			
			if (rc == LIBEVDEV_READ_STATUS_SYNC) {
				while (libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev) == LIBEVDEV_READ_STATUS_SYNC);
				continue;
			}

			if (ev.type == EV_KEY && ev.code == target_button) {
				if (ev.value == 1) {
					if (libevdev_has_event_type(dev, EV_ABS)) {
						virtual_x = libevdev_get_event_value(dev, EV_ABS, ABS_X);
						virtual_y = libevdev_get_event_value(dev, EV_ABS, ABS_Y);
					} else {
						virtual_x = 0;
						virtual_y = 0;
					}
					grabbing_start_movement(self, virtual_x, virtual_y);
				} else if (ev.value == 0) {
					grabbing_end_movement(self, virtual_x, virtual_y, (char*)libevdev_get_name(dev), conf);
				}
			} else {
				if (grabbed) {
					uinput_forward_event(ev.type, ev.code, ev.value);
				}
				if (ev.type == EV_REL) {
					if (ev.code == REL_X) {
						virtual_x += ev.value;
						moved = 1;
					} else if (ev.code == REL_Y) {
						virtual_y += ev.value;
						moved = 1;
					}
				} else if (ev.type == EV_ABS) {
					if (ev.code == ABS_X) {
						virtual_x = ev.value;
						moved = 1;
					} else if (ev.code == ABS_Y) {
						virtual_y = ev.value;
						moved = 1;
					}
				} else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
					if (moved && self->started) {
						grabbing_update_movement(self, virtual_x, virtual_y);
					}
					moved = 0;
				}
			}
		}

		if (rc != -EAGAIN && rc < 0) {
			fprintf(stderr, "Error reading event: %s\n", strerror(-rc));
			break;
		}
	}

	if (grabbed) {
		libevdev_grab(dev, LIBEVDEV_UNGRAB);
	}
	libevdev_free(dev);
	close(fd);
}