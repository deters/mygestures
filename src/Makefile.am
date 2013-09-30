
AM_CFLAGS = -DSYSCONFIR='"$(sysconfdir)"' $(XML_CFLAGS) -O4

bin_PROGRAMS=mygestures

mygestures_SOURCES = \
	main.c \
	gestures.c gestures.h \
	generic_wm.c wm.h \
	grabbing.c grabbing.h \
	drawing-backing.c \
	drawing-backing.h \
	drawing-brush.c \
	drawing-brush.h \
	drawing-brush-image.h \
	drawing-brush-shadow.h \
	drawing-brush-image.c \
	drawing-bresenham.c \
	drawing-bresenham.h

mygestures_LDADD=-lm $(XML_LIBS)
