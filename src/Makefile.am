
AM_CFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' -O2 $(libXML_CFLAGS) 

bin_PROGRAMS=mygestures

mygestures_SOURCES = \
	mygestures.c \
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


#SUBDIRS=drawing

mygestures_LDADD=$(libXML_LIBS) $(X11_LIBS) $(Xrender_LIBS) $(Xtst_LIBS) $(libXML_LIBS) $(Xi_LIBS)
