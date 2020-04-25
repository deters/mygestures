

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h> // for close
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

#include <libudev.h>
#include <libinput.h>

#include "config.h"

#include <errno.h>
#include <inttypes.h>
#include <getopt.h>
#include <poll.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "linux/input.h"
#include <libinput.h>

#include <libevdev/libevdev.h>

#include "libinput-shared.h"
#include "libinput-grabber.h"

#include "mygestures.h"

static uint32_t start_time;
static struct tools_options options;
static volatile sig_atomic_t stop = 0;
static bool be_quiet = true;

#define printq(...) ({ if (!be_quiet)  printf(__VA_ARGS__); })

static void
print_event_time(uint32_t time)
{
        printq("%+6.3fs	", start_time ? (time - start_time) / 1000.0 : 0);
}

static inline void
print_device_options(struct libinput_device *dev)
{
        uint32_t scroll_methods, click_methods;

        if (libinput_device_config_tap_get_finger_count(dev))
        {
                printq(" tap");
                if (libinput_device_config_tap_get_drag_lock_enabled(dev))
                        printq("(dl on)");
                else
                        printq("(dl off)");
        }
        if (libinput_device_config_left_handed_is_available(dev))
                printq(" left");
        if (libinput_device_config_scroll_has_natural_scroll(dev))
                printq(" scroll-nat");
        if (libinput_device_config_calibration_has_matrix(dev))
                printq(" calib");

        scroll_methods = libinput_device_config_scroll_get_methods(dev);
        if (scroll_methods != LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
        {
                printq(" scroll");
                if (scroll_methods & LIBINPUT_CONFIG_SCROLL_2FG)
                        printq("-2fg");
                if (scroll_methods & LIBINPUT_CONFIG_SCROLL_EDGE)
                        printq("-edge");
                if (scroll_methods & LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN)
                        printq("-button");
        }

        click_methods = libinput_device_config_click_get_methods(dev);
        if (click_methods != LIBINPUT_CONFIG_CLICK_METHOD_NONE)
        {
                printq(" click");
                if (click_methods & LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS)
                        printq("-buttonareas");
                if (click_methods & LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER)
                        printq("-clickfinger");
        }

        if (libinput_device_config_dwt_is_available(dev))
        {
                if (libinput_device_config_dwt_get_enabled(dev) ==
                    LIBINPUT_CONFIG_DWT_ENABLED)
                        printq(" dwt-on");
                else
                        printq(" dwt-off)");
        }

        if (libinput_device_has_capability(dev,
                                           LIBINPUT_DEVICE_CAP_TABLET_PAD))
        {
                int nbuttons, nstrips, nrings, ngroups;

                nbuttons = libinput_device_tablet_pad_get_num_buttons(dev);
                nstrips = libinput_device_tablet_pad_get_num_strips(dev);
                nrings = libinput_device_tablet_pad_get_num_rings(dev);
                ngroups = libinput_device_tablet_pad_get_num_mode_groups(dev);

                printq(" buttons:%d strips:%d rings:%d mode groups:%d",
                       nbuttons,
                       nstrips,
                       nrings,
                       ngroups);
        }
}

static void
print_device_notify(struct libinput_event *ev)
{
        struct libinput_device *dev = libinput_event_get_device(ev);
        struct libinput_seat *seat = libinput_device_get_seat(dev);
        struct libinput_device_group *group;
        double w, h;
        static int next_group_id = 0;
        intptr_t group_id;

        group = libinput_device_get_device_group(dev);
        group_id = (intptr_t)libinput_device_group_get_user_data(group);
        if (!group_id)
        {
                group_id = ++next_group_id;
                libinput_device_group_set_user_data(group, (void *)group_id);
        }

        printq("%-33s %5s %7s group%-2d",
               libinput_device_get_name(dev),
               libinput_seat_get_physical_name(seat),
               libinput_seat_get_logical_name(seat),
               (int)group_id);

        printq(" cap:");
        if (libinput_device_has_capability(dev,
                                           LIBINPUT_DEVICE_CAP_KEYBOARD))
                printq("k");
        if (libinput_device_has_capability(dev,
                                           LIBINPUT_DEVICE_CAP_POINTER))
                printq("p");
        if (libinput_device_has_capability(dev,
                                           LIBINPUT_DEVICE_CAP_TOUCH))
                printq("t");
        if (libinput_device_has_capability(dev,
                                           LIBINPUT_DEVICE_CAP_GESTURE))
                printq("g");
        if (libinput_device_has_capability(dev,
                                           LIBINPUT_DEVICE_CAP_TABLET_TOOL))
                printq("T");
        if (libinput_device_has_capability(dev,
                                           LIBINPUT_DEVICE_CAP_TABLET_PAD))
                printq("P");
        if (libinput_device_has_capability(dev,
                                           LIBINPUT_DEVICE_CAP_SWITCH))
                printq("S");

        if (libinput_device_get_size(dev, &w, &h) == 0)
                printq("  size %.0fx%.0fmm", w, h);

        if (libinput_device_has_capability(dev,
                                           LIBINPUT_DEVICE_CAP_TOUCH))
                printq(" ntouches %d", libinput_device_touch_get_touch_count(dev));

        if (libinput_event_get_type(ev) == LIBINPUT_EVENT_DEVICE_ADDED)
                print_device_options(dev);

        printq("\n");
}

static void
print_gesture_event_without_coords(struct libinput_event *ev)
{
        struct libinput_event_gesture *t = libinput_event_get_gesture_event(ev);
        int finger_count = libinput_event_gesture_get_finger_count(t);
        int cancelled = 0;
        enum libinput_event_type type;

        type = libinput_event_get_type(ev);

        if (type == LIBINPUT_EVENT_GESTURE_SWIPE_END ||
            type == LIBINPUT_EVENT_GESTURE_PINCH_END)
                cancelled = libinput_event_gesture_get_cancelled(t);

        print_event_time(libinput_event_gesture_get_time(t));
        printq("%d%s\n", finger_count, cancelled ? " cancelled" : "");
}

static void
print_gesture_event_with_coords(struct libinput_event *ev)
{
        struct libinput_event_gesture *t = libinput_event_get_gesture_event(ev);
        double dx = libinput_event_gesture_get_dx(t);
        double dy = libinput_event_gesture_get_dy(t);
        double dx_unaccel = libinput_event_gesture_get_dx_unaccelerated(t);
        double dy_unaccel = libinput_event_gesture_get_dy_unaccelerated(t);

        print_event_time(libinput_event_gesture_get_time(t));

        printq("%d %5.2f/%5.2f (%5.2f/%5.2f unaccelerated)",
               libinput_event_gesture_get_finger_count(t),
               dx, dy, dx_unaccel, dy_unaccel);

        if (libinput_event_get_type(ev) ==
            LIBINPUT_EVENT_GESTURE_PINCH_UPDATE)
        {
                double scale = libinput_event_gesture_get_scale(t);
                double angle = libinput_event_gesture_get_angle_delta(t);

                printq(" %5.2f @ %5.2f\n", scale, angle);
        }
        else
        {
                printq("\n");
        }
}

static int
handle_and_print_events(struct libinput *li, Mygestures *mygestures, LibinputGrabber *self)
{
        int rc = -1;
        struct libinput_event *ev;

        libinput_dispatch(li);
        while ((ev = libinput_get_event(li)))
        {

                //  print_event_header(ev);

                switch (libinput_event_get_type(ev))
                {
                case LIBINPUT_EVENT_NONE:
                        abort();
                case LIBINPUT_EVENT_DEVICE_ADDED:

                        if (libinput_device_has_capability(libinput_event_get_device(ev), LIBINPUT_DEVICE_CAP_GESTURE))
                        {
                                // char *event_device_name = strdup(libinput_device_get_sysname(libinput_event_get_device(ev)));
                                // printf("%s\t", event_device_name);

                                printf("%-33s ",
                                       libinput_device_get_name(libinput_event_get_device(ev)));
                        }
                        //tools_device_apply_config(libinput_event_get_device(ev),
                        //                          &options);
                        break;
                case LIBINPUT_EVENT_DEVICE_REMOVED:

                        if (strcmp(self->devicename, libinput_device_get_name(libinput_event_get_device(ev))) == 0)
                        {
                                // print_gesture_event_without_coords(ev);
                                // grabbing_end_movement(mygestures, 0, 0, "", mygestures);

                                print_device_notify(ev);

                                self->devicename = "";
                                self->event_count = 0;
                        }

                        break;

                case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:

                        if (self->event_count == 0)
                        {
                                self->delta_min = 50;

                                self->devicename = strdup(libinput_device_get_name(libinput_event_get_device(ev)));

                                print_gesture_event_without_coords(ev);

                                if (self->nfingers == libinput_event_gesture_get_finger_count(libinput_event_get_gesture_event(ev)))
                                {

                                        // já está enviando a diferença (delta).
                                        mygestures_set_delta_updates(mygestures, 1);
                                        mygestures_start_movement(mygestures, 0, 0, self->delta_min);

                                        // mygestures_start_movement(mygestures, ev)
                                }
                        }

                        break;
                case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
                        assert(self->devicename);
                        // struct libinput_event_gesture *t = libinput_event_get_gesture_event(ev);
                        //double dx = libinput_event_gesture_get_dx(t);
                        //double dy = libinput_event_gesture_get_dy(t);
                        // double dx_unaccel = libinput_event_gesture_get_dx_unaccelerated(t);
                        // double dy_unaccel = libinput_event_gesture_get_dy_unaccelerated(t);

                        if (strcmp(self->devicename, libinput_device_get_name(libinput_event_get_device(ev))) == 0)
                        {

                                if (self->nfingers == libinput_event_gesture_get_finger_count(libinput_event_get_gesture_event(ev)))
                                {

                                        print_gesture_event_with_coords(ev);

                                        self->event_count = self->event_count + 1;

                                        if (self->event_count > 1)
                                        { /// ignora o primeiro movimento, pois está calculando errado.
                                                mygestures_update_movement(mygestures, libinput_event_gesture_get_dx_unaccelerated(libinput_event_get_gesture_event(ev)), libinput_event_gesture_get_dy_unaccelerated(libinput_event_get_gesture_event(ev)), self->delta_min);
                                        }
                                }
                        }
                        break;
                case LIBINPUT_EVENT_GESTURE_SWIPE_END:
                        assert(self->devicename);

                        if (strcmp(self->devicename, libinput_device_get_name(libinput_event_get_device(ev))) == 0)
                        {

                                print_gesture_event_without_coords(ev);

                                if (self->nfingers == libinput_event_gesture_get_finger_count(libinput_event_get_gesture_event(ev)))
                                {

                                        mygestures_end_movement(mygestures, libinput_event_gesture_get_cancelled(libinput_event_get_gesture_event(ev)), self->devicename);

                                        self->devicename = "";
                                        self->event_count = 0;
                                }
                        }

                        break;
                case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
                        print_gesture_event_without_coords(ev);
                        break;
                case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
                        print_gesture_event_with_coords(ev);
                        break;
                case LIBINPUT_EVENT_GESTURE_PINCH_END:
                        print_gesture_event_without_coords(ev);
                        break;
                case LIBINPUT_EVENT_TABLET_TOOL_AXIS:
                case LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY:
                case LIBINPUT_EVENT_TABLET_TOOL_TIP:
                case LIBINPUT_EVENT_TABLET_TOOL_BUTTON:
                case LIBINPUT_EVENT_TABLET_PAD_BUTTON:
                case LIBINPUT_EVENT_TABLET_PAD_RING:
                case LIBINPUT_EVENT_TABLET_PAD_STRIP:
                case LIBINPUT_EVENT_TABLET_PAD_KEY:
                case LIBINPUT_EVENT_SWITCH_TOGGLE:
                case LIBINPUT_EVENT_KEYBOARD_KEY:
                case LIBINPUT_EVENT_POINTER_MOTION:
                case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
                case LIBINPUT_EVENT_POINTER_BUTTON:
                case LIBINPUT_EVENT_POINTER_AXIS:
                case LIBINPUT_EVENT_TOUCH_DOWN:
                case LIBINPUT_EVENT_TOUCH_MOTION:
                case LIBINPUT_EVENT_TOUCH_UP:
                case LIBINPUT_EVENT_TOUCH_CANCEL:
                case LIBINPUT_EVENT_TOUCH_FRAME:
                        break;
                }

                libinput_event_destroy(ev);
                libinput_dispatch(li);
                rc = 0;
        }
        return rc;
}

static void
sighandler(int signal, siginfo_t *siginfo, void *userdata)
{
        stop = 1;
}

static void
mainloop(struct libinput *li, Mygestures *mygestures, LibinputGrabber *self)
{
        struct pollfd fds;

        fds.fd = libinput_get_fd(li);
        fds.events = POLLIN;
        fds.revents = 0;

        /* Handle already-pending device added events */
        if (handle_and_print_events(li, mygestures, self))
                fprintf(stderr, "Expected device added events on startup but got none. "
                                "Maybe you don't have the right permissions?\n");

        /* time offset starts with our first received event */
        if (poll(&fds, 1, -1) > -1)
        {
                struct timespec tp;

                clock_gettime(CLOCK_MONOTONIC, &tp);
                start_time = tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
                do
                {
                        handle_and_print_events(li, mygestures, self);
                } while (!stop && poll(&fds, 1, -1) > -1);
        }

        printf("\n");
}

void libinput_grabber_loop(LibinputGrabber *self, Mygestures *mygestures)
{

        struct libinput *li;
        enum tools_backend backend = BACKEND_NONE;
        const char *seat_or_devices[60] = {NULL};
        bool grab = false;
        bool verbose = false;
        struct sigaction act;

        tools_init_options(&options);

        backend = BACKEND_UDEV;
        seat_or_devices[0] = "seat0";

        memset(&act, 0, sizeof(act));
        act.sa_sigaction = sighandler;
        act.sa_flags = SA_SIGINFO;

        if (sigaction(SIGINT, &act, NULL) == -1)
        {
                fprintf(stderr, "Failed to set up signal handling (%s)\n",
                        strerror(errno));
                return; // EXIT_FAILURE;
        }

        li = tools_open_backend(backend, seat_or_devices, verbose, &grab);
        if (!li)
                return; // EXIT_FAILURE;

        mainloop(li, mygestures, self);

        libinput_unref(li);

        return; // EXIT_SUCCESS;
}

LibinputGrabber *libinput_grabber_new(char *device_name, int nfingers)
{

        LibinputGrabber *self = malloc(sizeof(LibinputGrabber));
        bzero(self, sizeof(LibinputGrabber));

        assert(device_name);
        //assert(button);

        self->devicename = strdup(device_name);

        self->nfingers = nfingers;
        return self;
}

void libinput_grabber_list_devices()
{
        struct libinput *li;
        struct libinput_event *ev;
        bool grab = false;
        const char *seat[2] = {"seat0", NULL};

        li = tools_open_backend(BACKEND_UDEV, seat, false, &grab);
        if (!li)
        {
                exit(1);
                printf("Error opening backend udev\n");
        }

        printf("\nLibinput devices with gesture capabilities:\n");

        libinput_dispatch(li);
        while ((ev = libinput_get_event(li)))
        {
                if (libinput_event_get_type(ev) == LIBINPUT_EVENT_DEVICE_ADDED)
                {

                        if (
                            libinput_device_has_capability(libinput_event_get_device(ev), LIBINPUT_DEVICE_CAP_GESTURE))
                        {
                                //      char *event_device_name = strdup(libinput_device_get_sysname(libinput_event_get_device(ev)));
                                // printf("%s\t", event_device_name);
                                printf("  [x] '%s'\n",
                                       libinput_device_get_name(libinput_event_get_device(ev)));
                        }
                }

                libinput_event_destroy(ev);
                libinput_dispatch(li);
        }

        printf("\n");

        libinput_unref(li);
}

void libinput_grabber_loop(LibinputGrabber *self, Mygestures *mygestures);
void grabber_libinput_finalize(LibinputGrabber *self);
