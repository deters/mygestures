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
#include <X11/Xlib.h>

#include "grabbing.h"
#include "grabbing-evdev.h"

int find_mouse_device(char *path, size_t len) {
    DIR *dir;
    struct dirent *entry;
    const char *input_dir = "/dev/input/by-path/";

    dir = opendir(input_dir);
    if (!dir) {
        perror("opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Look for entries that indicate a mouse (e.g., '-mouse').
        if (strstr(entry->d_name, "-mouse")) {
            snprintf(path, len, "/dev/input/by-path/%s", entry->d_name);
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    fprintf(stderr, "No mouse device found in %s\n", input_dir);
    return -1;
}

static int get_evdev_button_code(int button) {
	switch (button) {
		case 1:
			return BTN_LEFT;
		case 2:
			return BTN_MIDDLE;
		case 3:
			return BTN_RIGHT;
		default:
			if (button > 3) return button;
			return BTN_LEFT;
	}
}

static void get_pointer_position(Display *dpy, int *x, int *y) {
	Window root_return, child_return;
	int root_x_return, root_y_return;
	int win_x_return, win_y_return;
	unsigned int mask_return;
	XQueryPointer(dpy, DefaultRootWindow(dpy), &root_return, &child_return,
				  &root_x_return, &root_y_return, &win_x_return, &win_y_return,
				  &mask_return);
	*x = root_x_return;
	*y = root_y_return;
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
		fprintf(stderr, "Did you run the program with 'sudo'?\n");
		return;
	}

	rc = libevdev_new_from_fd(fd, &dev);
	if (rc < 0) {
		fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
		close(fd);
		return;
	}

	printf("Listening for events from %s using libevdev\n", libevdev_get_name(dev));

	int target_button = get_evdev_button_code(self->button);
	int moved = 0;

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
		while ((rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) == LIBEVDEV_READ_STATUS_SUCCESS) {
			if (ev.type == EV_KEY && ev.code == target_button) {
				int x, y;
				get_pointer_position(self->dpy, &x, &y);
				if (ev.value == 1) {
					grabbing_start_movement(self, x, y);
				} else if (ev.value == 0) {
					grabbing_end_movement(self, x, y, (char*)libevdev_get_name(dev), conf);
				}
			} else if (ev.type == EV_REL && (ev.code == REL_X || ev.code == REL_Y)) {
				moved = 1;
			} else if (ev.type == EV_ABS && (ev.code == ABS_X || ev.code == ABS_Y)) {
				moved = 1;
			} else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
				if (moved && self->started) {
					int x, y;
					get_pointer_position(self->dpy, &x, &y);
					grabbing_update_movement(self, x, y);
				}
				moved = 0;
			}
		}

		if (rc != -EAGAIN && rc < 0) {
			fprintf(stderr, "Error reading event: %s\n", strerror(-rc));
			break;
		}
	}

	libevdev_free(dev);
	close(fd);
}
