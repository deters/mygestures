/*
 * Copyright © 2014 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <config.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libevdev/libevdev.h>

#include "libinput-shared.h"

LIBINPUT_ATTRIBUTE_PRINTF(3, 0)
static void
log_handler(struct libinput *li,
            enum libinput_log_priority priority,
            const char *format,
            va_list args)
{
}

void tools_init_options(struct tools_options *options)
{
    memset(options, 0, sizeof(*options));
    options->tapping = -1;
    options->tap_map = -1;
    options->drag = -1;
    options->drag_lock = -1;
    options->natural_scroll = -1;
    options->left_handed = -1;
    options->middlebutton = -1;
    options->dwt = -1;
    options->click_method = -1;
    options->scroll_method = -1;
    options->scroll_button = -1;
    options->scroll_button_lock = -1;
    options->speed = 0.0;
    options->profile = LIBINPUT_CONFIG_ACCEL_PROFILE_NONE;
}

static int
open_restricted(const char *path, int flags, void *user_data)
{
    bool *grab = user_data;
    int fd = open(path, flags);

    if (fd < 0)
        fprintf(stderr, "Failed to open %s (%s)\n",
                path, strerror(errno));
    else if (grab && *grab && ioctl(fd, EVIOCGRAB, (void *)1) == -1)
        fprintf(stderr, "Grab requested, but failed for %s (%s)\n",
                path, strerror(errno));

    return fd < 0 ? -errno : fd;
}

static void
close_restricted(int fd, void *user_data)
{
    close(fd);
}

static const struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

static struct libinput *
tools_open_udev(const char *seat, bool verbose, bool *grab)
{
    struct libinput *li;
    struct udev *udev = udev_new();

    if (!udev)
    {
        fprintf(stderr, "Failed to initialize udev\n");
        return NULL;
    }

    li = libinput_udev_create_context(&interface, grab, udev);
    if (!li)
    {
        fprintf(stderr, "Failed to initialize context from udev\n");
        goto out;
    }

    libinput_log_set_handler(li, log_handler);
    if (verbose)
        libinput_log_set_priority(li, LIBINPUT_LOG_PRIORITY_DEBUG);

    if (libinput_udev_assign_seat(li, seat))
    {
        fprintf(stderr, "Failed to set seat\n");
        libinput_unref(li);
        li = NULL;
        goto out;
    }

out:
    udev_unref(udev);
    return li;
}

static struct libinput *
tools_open_device(const char **paths, bool verbose, bool *grab)
{
    struct libinput_device *device;
    struct libinput *li;
    const char **p = paths;

    li = libinput_path_create_context(&interface, grab);
    if (!li)
    {
        fprintf(stderr, "Failed to initialize path context\n");
        return NULL;
    }

    if (verbose)
    {
        libinput_log_set_handler(li, log_handler);
        libinput_log_set_priority(li, LIBINPUT_LOG_PRIORITY_DEBUG);
    }

    while (*p)
    {
        device = libinput_path_add_device(li, *p);
        if (!device)
        {
            fprintf(stderr, "Failed to initialize device %s\n", *p);
            libinput_unref(li);
            li = NULL;
            break;
        }
        p++;
    }

    return li;
}

struct libinput *
tools_open_backend(enum tools_backend which,
                   const char **seat_or_device,
                   bool verbose,
                   bool *grab)
{
    struct libinput *li;

    //tools_setenv_quirks_dir();

    switch (which)
    {
    case BACKEND_UDEV:
        li = tools_open_udev(seat_or_device[0], verbose, grab);
        break;
    case BACKEND_DEVICE:
        li = tools_open_device(seat_or_device, verbose, grab);
        break;
    default:
        abort();
    }

    return li;
}

static char *
find_device(const char *udev_tag)
{
    struct udev *udev;
    struct udev_enumerate *e;
    struct udev_list_entry *entry = NULL;
    struct udev_device *device;
    const char *path, *sysname;
    char *device_node = NULL;

    udev = udev_new();
    e = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(e, "input");
    udev_enumerate_scan_devices(e);

    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e))
    {
        path = udev_list_entry_get_name(entry);
        device = udev_device_new_from_syspath(udev, path);
        if (!device)
            continue;

        sysname = udev_device_get_sysname(device);
        if (strncmp("event", sysname, 5) != 0)
        {
            udev_device_unref(device);
            continue;
        }

        if (udev_device_get_property_value(device, udev_tag))
            device_node = strdup(udev_device_get_devnode(device));

        udev_device_unref(device);

        if (device_node)
            break;
    }
    udev_enumerate_unref(e);
    udev_unref(udev);

    return device_node;
}

bool find_touchpad_device(char *path, size_t path_len)
{
    char *devnode = find_device("ID_INPUT_TOUCHPAD");

    if (devnode)
    {
        snprintf(path, path_len, "%s", devnode);
        free(devnode);
    }

    return devnode != NULL;
}

bool is_touchpad_device(const char *devnode)
{
    struct udev *udev;
    struct udev_device *dev = NULL;
    struct stat st;
    bool is_touchpad = false;

    if (stat(devnode, &st) < 0)
        return false;

    udev = udev_new();
    dev = udev_device_new_from_devnum(udev, 'c', st.st_rdev);
    if (!dev)
        goto out;

    is_touchpad = udev_device_get_property_value(dev, "ID_INPUT_TOUCHPAD");
out:
    if (dev)
        udev_device_unref(dev);
    udev_unref(udev);

    return is_touchpad;
}
