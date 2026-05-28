#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([MyGestures], [3.1.0], [lucasdeters@gmail.com])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_SRCDIR([src/mygestures.c])
AC_CONFIG_HEADERS([src/config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(Xrender, xrender)
PKG_CHECK_MODULES(Xtst, xtst)
PKG_CHECK_MODULES(Xi, xi)
PKG_CHECK_MODULES(libyaml, yaml-0.1)
PKG_CHECK_MODULES(libevdev, libevdev)

# Check for GTK4
AC_ARG_ENABLE([ui],
    [AS_HELP_STRING([--disable-ui], [Disable the GTK UI (gestos)])],
    [enable_ui=$enableval],
    [enable_ui=yes])

if test "x$enable_ui" = "xyes"; then
    PKG_CHECK_MODULES(GTK, [gtk4])
fi

AM_CONDITIONAL([ENABLE_UI], [test "x$enable_ui" = "xyes"])

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
