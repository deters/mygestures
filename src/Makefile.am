
AM_CFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' -O2 $(LIBXML2_CFLAGS) $(LIBEVDEV_CFLAGS) \
  $(LIBUDEV_CFLAGS) $(LIBINPUT_CFLAGS) -Wall -Wunreachable-code # -fprofile-arcs -ftest-coverage

bin_PROGRAMS=gestos touchgestures mousegestures

common = \
        gestures.c gestures.h \
        configuration.c configuration.h \
        configuration_parser.c configuration_parser.h 

libinput = \
	libinput-grabber.c libinput-grabber.h \
	libinput-shared.c libinput-shared.h 

xinput = \
	xinput-grabber.c xinput-grabber.h 

touchgestures_SOURCES = \
	$(common) $(libinput) touchgestures.c 


mousegestures_SOURCES = \
	$(common) $(xinput) mousegestures.c

gestos_SOURCES = \
	$(common) $(xinput) $(libinput) gestos.c


common_LDADD=$(LIBXML2_LIBS) $(LIBX11_LIBS) $(LIBXRENDER_LIBS) $(LIBXTST_LIBS) \
 $(LIBXML_LIBS) $(LIBXI_LIBS) $(LIBEVDEV_LIBS) -lm
touchgestures_LDADD=$(common_LDADD) $(LIBINPUT_LIBS) $(LIBUDEV_LIBS)
mousegestures_LDADD=$(common_LDADD)
gestos_LDADD=$(touchgestures_LDADD) $(mousegestures_LDADD)
