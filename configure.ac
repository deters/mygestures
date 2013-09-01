dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(Gnu1, 1.0)


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

      

AC_PROG_CC


AC_CONFIG_FILES(Makefile src/Makefile)
AC_OUTPUT

