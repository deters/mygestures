
AM_CFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' -O2 $(LIBXML2_CFLAGS) $(LIBEVDEV_CFLAGS) \ 
  $(LIBUDEV_CFLAGS)  -Wall -Wunreachable-code # -fprofile-arcs -ftest-coverage

bin_PROGRAMS=mygestures touchgestures
#  igestures


common_SOURCES = \
	      mygestures.c mygestures.h \
	      configuration.c configuration.h \
        configuration_parser.c configuration_parser.h \
	      actions.c actions.h \
        grabbing-libinput.c grabbing-libinput.h \
	      grabbing-xinput.c grabbing-xinput.h \
	      grabbing-synaptics.c grabbing-synaptics.h \
        drawing/drawing-backing.c \
        drawing/drawing-backing.h \
        drawing/drawing-brush.c \
        drawing/drawing-brush.h \
        drawing/drawing-brush-image.h \
        drawing/drawing-brush-shadow.h \
        drawing/drawing-brush-image.c \
        drawing/drawing-bresenham.c \
        drawing/drawing-bresenham.h shared.c shared.h
	

mygestures_SOURCES = \
        main.c \
	$(common_SOURCES)

touchgestures_SOURCES = \
	touchgestures.c $(common_SOURCES)

common_LDADD=$(LIBXML2_LIBS) $(LIBX11_LIBS) $(LIBXRENDER_LIBS) $(LIBXTST_LIBS) \
 $(LIBXML_LIBS) $(LIBXI_LIBS) $(LIBEVDEV_LIBS) $(LIBUDEV_LIBS) $(LIBINPUT_LIBS) -lm

mygestures_LDADD=$(common_LDADD)

touchgestures_LDADD=$(common_LDADD)



# igestures_SOURCES = grabbing-libinput.c shared.h shared.c

# igestures_LDADD=$(LIBINPUT_LIBS) $(LIBEVDEV_LIBS) $(LIBUDEV_LIBS)

#SUBDIRS=drawing
