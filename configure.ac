dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(MyGestures, 0.9)


AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE()

# Checks for libraries.
AC_CHECK_LIB([X11], [XGrabButton],,
        AC_MSG_ERROR([*** libX11 not found. Tip: install libx11-dev ]),
        $x_libs_for_checks)
        
AC_CHECK_LIB(X11, XkbKeycodeToKeysym,,
        AC_MSG_ERROR([*** XkbKeycodeToKeysym not found in libX11. Check 'config.log' for more details.]),	        
        $x_libs_for_checks)
AC_CHECK_LIB([Xmu], [XmuClientWindow],,
        AC_MSG_ERROR([*** libXmu not found. Tip: install libxmu-dev]),
        [$x_libs_for_checks])
AC_CHECK_LIB([Xrender], [XRenderComposite],,
        AC_MSG_ERROR([*** libXrender not found. Tip: install libxrender-dev]),
        [$x_libs_for_checks])
AC_CHECK_LIB([Xtst], [XTestFakeKeyEvent],,
        AC_MSG_ERROR([*** libXtst not found. Tip: install libxtst-dev]),
        [$x_libs_for_checks])
PKG_CHECK_MODULES(XML, libxml-2.0 >= 2.4)      


AC_CHECK_FUNCS([bzero])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([mkdir])
AC_CHECK_FUNCS([regcomp])
AC_CHECK_FUNCS([strcasecmp])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strncasecmp])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([strings.h])
AC_CHECK_HEADERS([unistd.h])
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_PATH_X
AC_TYPE_MODE_T



AC_PROG_CC


AC_CONFIG_FILES(Makefile src/Makefile)
AC_OUTPUT

