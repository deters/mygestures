
AM_CFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' -O2 $(libyaml_CFLAGS) $(libevdev_CFLAGS)

bin_PROGRAMS=mygestures

if ENABLE_UI
bin_PROGRAMS += gestos
endif

mygestures_SOURCES = \
    main.c \
	ipc.c ipc.h \
	mygestures.c mygestures.h \
	configuration.c configuration.h \
    configuration_parser.c configuration_parser.h \
	actions.c actions.h \
	wayland_context.c wayland_context.h \
	action_backend.h action_backend_wayland.c \
	grabbing.c grabbing.h \
	wayland.c wayland.h \
	grabbing-evdev.c grabbing-evdev.h \
	uinput_device.c uinput_device.h \
	logging.h \
	main.h

gestos_SOURCES = \
	gestos.c \
	ipc.c ipc.h \
	configuration.c configuration.h \
	configuration_parser.c configuration_parser.h \
	actions.c actions.h \
	wayland_context.c wayland_context.h \
	action_backend.h action_backend_wayland.c \
	mygestures.c mygestures.h \
	grabbing.c grabbing.h \
	wayland.c wayland.h \
	grabbing-evdev.c grabbing-evdev.h \
	uinput_device.c uinput_device.h \
	logging.h \
	main.h

gestos_CFLAGS = $(AM_CFLAGS) $(GTK_CFLAGS)
gestos_LDADD = $(mygestures_LDADD) $(GTK_LIBS)

mygestures_LDADD=$(libyaml_LIBS) $(libevdev_LIBS) -lm
