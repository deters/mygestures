
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
	    grabbing.c grabbing.h \
	    wayland.c wayland.h \
	    x11_window.c x11_window.h \
	    grabbing-synaptics.c grabbing-synaptics.h \
	    grabbing-evdev.c grabbing-evdev.h \
	    uinput_device.c uinput_device.h \
        drawing/drawing-backing.c \
        drawing/drawing-backing.h \
        drawing/drawing-brush.c \
        drawing/drawing-brush.h \
        drawing/drawing-brush-image.h \
        drawing/drawing-brush-shadow.h \
        drawing/drawing-brush-image.c \
        drawing/drawing-bresenham.c \
        drawing/drawing-bresenham.h

gestos_SOURCES = \
	gestos.c \
	ipc.c ipc.h \
	configuration.c configuration.h \
	configuration_parser.c configuration_parser.h \
	actions.c actions.h \
	mygestures.c mygestures.h \
	grabbing.c grabbing.h \
	wayland.c wayland.h \
	x11_window.c x11_window.h \
	grabbing-synaptics.c grabbing-synaptics.h \
	grabbing-evdev.c grabbing-evdev.h \
	uinput_device.c uinput_device.h \
	drawing/drawing-backing.c \
	drawing/drawing-backing.h \
	drawing/drawing-brush.c \
	drawing/drawing-brush.h \
	drawing/drawing-brush-image.h \
	drawing/drawing-brush-shadow.h \
	drawing/drawing-brush-image.c \
	drawing/drawing-bresenham.c \
	drawing/drawing-bresenham.h

gestos_CFLAGS = $(AM_CFLAGS) $(GTK_CFLAGS)
gestos_LDADD = $(mygestures_LDADD) $(GTK_LIBS)

#SUBDIRS=drawing

mygestures_LDADD=$(libyaml_LIBS) $(X11_LIBS) $(Xrender_LIBS) $(Xtst_LIBS) $(Xi_LIBS) $(libevdev_LIBS) -lm
