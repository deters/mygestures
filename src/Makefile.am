
AM_CFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' -O2 $(LIBXML2_CFLAGS) $(LIBEVDEV_CFLAGS) \
  $(LIBUDEV_CFLAGS)  -Wall -Wunreachable-code # -fprofile-arcs -ftest-coverage

bin_PROGRAMS=touchgestures mousegestures


common_SOURCES = \
        gestures.c gestures.h \
        configuration.c configuration.h \
        configuration_parser.c configuration_parser.h \
        drawing/drawing-backing.c \
        drawing/drawing-backing.h \
        drawing/drawing-brush.c \
        drawing/drawing-brush.h \
        drawing/drawing-brush-image.h \
        drawing/drawing-brush-shadow.h \
        drawing/drawing-brush-image.c \
        drawing/drawing-bresenham.c \
	drawing/drawing-bresenham.h
	

touchgestures_SOURCES = \
	touchgestures.c  \
	libinput-grabber.c libinput-grabber.h \
	libinput-shared.c libinput-shared.h \
	$(common_SOURCES)


mousegestures_SOURCES = \
	mousegestures.c \
	xinput-grabber.c xinput-grabber.h  \
	$(common_SOURCES)
	

common_LDADD=$(LIBXML2_LIBS) $(LIBX11_LIBS) $(LIBXRENDER_LIBS) $(LIBXTST_LIBS) \
 $(LIBXML_LIBS) $(LIBXI_LIBS) $(LIBEVDEV_LIBS) -lm

touchgestures_LDADD=$(common_LDADD) $(LIBINPUT_LIBS) $(LIBUDEV_LIBS)
mousegestures_LDADD=$(common_LDADD)



# igestures_SOURCES = grabbing-libinput.c shared.h shared.c

# igestures_LDADD=$(LIBINPUT_LIBS) $(LIBEVDEV_LIBS) $(LIBUDEV_LIBS)

#SUBDIRS=drawing
