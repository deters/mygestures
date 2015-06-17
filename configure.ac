#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([MyGestures], [1.3], [lucasdeters@users.sourceforge.net])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/config.h])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.
PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(Xrender, xrender)
PKG_CHECK_MODULES(Xtst, xtst)
PKG_CHECK_MODULES(libXML, libxml-2.0 >= 2.4)

AC_SEARCH_LIBS([shm_open], [rt], [])

# Checks for header files.
AC_PATH_X
AC_CHECK_HEADERS

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_MODE_T
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([bzero ftruncate memset mkdir regcomp strcasecmp strdup])

# "--enable-debug" flag

AC_ARG_ENABLE(debug,
[  --enable-debug          Enable debugging (disabled by default)],
[if test x$enableval = xyes; then
    AC_DEFINE(DEBUG, 1, [Define this to enable debug messages and assert warnings.]) debug_support=yes
fi])

if test x$debug_support != xyes; then
    AC_DEFINE(NDEBUG, 1, [Shut up assert warnings :-)])
fi

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
