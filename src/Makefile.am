bin_PROGRAMS=mygestures

CC=clang

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
# ogdl/graph.c ogdl/ogdl.h ogdl/ogdlbin.c ogdl/ogdllog.c ogdl/ogdlparser.c ogdl/path.c

	
mygestures_LDADD=-lm $(XML_LIBS)

mygestures_CPPFLAGS = $(XML_CFLAGS)