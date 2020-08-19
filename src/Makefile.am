
AM_CFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' -O2 $(LIBXML2_CFLAGS) $(LIBEVDEV_CFLAGS) \
  $(LIBUDEV_CFLAGS) $(LIBINPUT_CFLAGS) -Wall -Wunreachable-code # -fprofile-arcs -ftest-coverage

bin_PROGRAMS=gestos

common = \
        gestures.c gestures.h \
        configuration.c configuration.h \
        configuration_parser.c configuration_parser.h 

libinput = \
	libinput-grabber.c libinput-grabber.h \
	libinput-shared.c libinput-shared.h

xinput = \
	xinput-grabber.c xinput-grabber.h

gestos_SOURCES = \
	$(common) $(xinput) $(libinput) gestos.c


gestos_LDADD=$(LIBXML2_LIBS) $(LIBX11_LIBS) $(LIBXRENDER_LIBS) $(LIBXTST_LIBS) \
 $(LIBXML_LIBS) $(LIBXI_LIBS) $(LIBEVDEV_LIBS) $(LIBINPUT_LIBS) $(LIBUDEV_LIBS) -lm 
