
AM_CFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' -O2 $(LIBXML2_CFLAGS) $(LIBEVDEV_CFLAGS) \ 
  $(LIBUDEV_CFLAGS)  -Wall -Wunreachable-code # -fprofile-arcs -ftest-coverage

bin_PROGRAMS=mygestures igestures

mygestures_SOURCES = \
    main.c main.h \
	mygestures.c mygestures.h \
	configuration.c configuration.h \
        configuration_parser.c configuration_parser.h \
	    actions.c actions.h \
	    grabbing.c grabbing.h \
	    grabbing-synaptics.c grabbing-synaptics.h \
        drawing/drawing-backing.c \
        drawing/drawing-backing.h \
        drawing/drawing-brush.c \
        drawing/drawing-brush.h \
        drawing/drawing-brush-image.h \
        drawing/drawing-brush-shadow.h \
        drawing/drawing-brush-image.c \
        drawing/drawing-bresenham.c \
        drawing/drawing-bresenham.h



mygestures_LDADD=$(LIBXML2_LIBS) $(LIBX11_LIBS) $(LIBXRENDER_LIBS) $(LIBXTST_LIBS) \
 $(LIBXML_LIBS) $(LIBXI_LIBS) -lm


igestures_SOURCES = grabbing-libinput.c shared.h shared.c

igestures_LDADD=$(LIBINPUT_LIBS) $(LIBEVDEV_LIBS) $(LIBUDEV_LIBS)

#SUBDIRS=drawing
