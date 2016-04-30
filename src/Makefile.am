
AM_CFLAGS = -DSYSCONFIR='"$(sysconfdir)"' -O2 $(libXML_CFLAGS) 

bin_PROGRAMS=mygestures

mygestures_SOURCES = \
	main.c \
	gestures.c gestures.h \
        configuration.c configuration.h \
	generic_wm.c wm.h \
	grabbing.c grabbing.h \
        drawing/drawing-backing.c \
        drawing/drawing-backing.h \
        drawing/drawing-brush.c \
        drawing/drawing-brush.h \
        drawing/drawing-brush-image.h \
        drawing/drawing-brush-shadow.h \
        drawing/drawing-brush-image.c \
        drawing/drawing-bresenham.c \
        drawing/drawing-bresenham.h


#SUBDIRS=drawing

mygestures_LDADD=$(libXML_LIBS) $(X11_LIBS) $(Xrender_LIBS) $(Xtst_LIBS) $(libXML_LIBS) $(Xi_LIBS)
