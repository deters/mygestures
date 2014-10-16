
AM_CFLAGS = -DSYSCONFIR='"$(sysconfdir)"' -O2 $(libXML_CFLAGS) 

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

mygestures_LDADD=$(libXML_LIBS) $(X11_LIBS) $(Xrender_LIBS) $(Xtst_LIBS) $(libXML_LIBS)
