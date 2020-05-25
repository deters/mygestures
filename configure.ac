#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Gesto], [1.0], [lucasdeters@gmail.com])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_SRCDIR([src/gestures.c])
AC_CONFIG_HEADERS([src/config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES(LIBX11, x11)
PKG_CHECK_MODULES(LIBXRENDER, xrender)
PKG_CHECK_MODULES(LIBXTST, xtst)
PKG_CHECK_MODULES(LIBXI, xi)
PKG_CHECK_MODULES(LIBXML2, libxml-2.0)
PKG_CHECK_MODULES(LIBINPUT, libinput)
PKG_CHECK_MODULES(LIBUDEV, libudev)
PKG_CHECK_MODULES(LIBEVDEV, libevdev)

AC_SEARCH_LIBS([shm_open], [rt], [])

# Checks for header files.
AC_PATH_X
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h strings.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_MODE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([bzero memset mkdir regcomp strcasecmp strdup strncasecmp])          

AC_CONFIG_FILES([Makefile
                 src/Makefile])
                 
                 

            
AC_OUTPUT


