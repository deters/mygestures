dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(MyGestures, 0.9)


AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE()

# Checks for libraries.
PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(Xmu, xmu)
PKG_CHECK_MODULES(Xrender, xrender)
PKG_CHECK_MODULES(Xtst, xtst)
PKG_CHECK_MODULES(libXML, libxml-2.0 >= 2.4)      

AC_PROG_CC

AC_CONFIG_FILES(Makefile src/Makefile)
AC_OUTPUT

