#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(had, 0.3.0, bjoern@bjoern-b.de)
m4_define([package_major_version], [0])
m4_define([package_minor_version], [3])
m4_define([package_micro_version], [0])
m4_define([package_version], [package_major_version.package_minor_version.package_micro_version])
AM_INIT_AUTOMAKE(had, 0.3.0)
AC_CONFIG_SRCDIR([src/had.h])

HAD_LIBTOOL_VERSION=1:0:0
AC_SUBST(had_LIBTOOL_VERSION)
AC_SUBST(EXTRA_CFLAGS)
AC_SUBST(EXTRA_LDFLAGS)

#glib
PKG_CHECK_MODULES([glib], glib-2.0 >= 2.10)
AC_SUBST(glib_LIBS)
AC_SUBST(glib_CFLAGS)
PKG_CHECK_MODULES([gmodule], gmodule-2.0 >= 2.10)
AC_SUBST(gmodule_LIBS)
AC_SUBST(gmodule_CFLAGS)

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h netinet/in.h stdint.h stdlib.h string.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT16_T
AC_TYPE_INT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([socket])
AC_OUTPUT(Makefile src/Makefile src/plugins/base_station/Makefile src/plugins/Makefile)

