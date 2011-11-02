#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(had, 0.3.0, bjoern@bjoern-b.de)
AM_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE(had, 0.3.0)
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL
# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h unistd.h inet.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_INT16_T
AC_TYPE_INT8_T
AC_STRUCT_TM
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([floor memset strchr strstr inet_aton getnameinfo])

AC_SUBST(EXTRA_CFLAGS)
AC_SUBST(EXTRA_LDFLAGS)
#glib
PKG_CHECK_MODULES([glib], glib-2.0 >= 2.10)
AC_SUBST(glib_LIBS)
AC_SUBST(glib_CFLAGS)

PKG_CHECK_MODULES([gthread], gthread-2.0 >= 2.10)
AC_SUBST(gthread_LIBS)
AC_SUBST(gthread_CFLAGS)

PKG_CHECK_MODULES([libcrypto], libcrypto >= 0.9.8m)
AC_SUBST(libcrypto_LIBS)
AC_SUBST(libcrypto_CFLAGS)

PKG_CHECK_MODULES([libmpd], libmpd >= 0.19.0)
AC_SUBST(libmpd_LIBS)
AC_SUBST(libmpd_CFLAGS)

PKG_CHECK_MODULES([libcurl], libcurl >= 7.0.0)
AC_SUBST(libcurl_LIBS)
AC_SUBST(libcurl_CFLAGS)

AC_ARG_ENABLE(fftw3,
              [  --disable-fftw3			Support for ledmatrix mpd fft.],
              [ case "${enableval}" in
                    yes)    enable_fftw3=yes;;
                     no)    enable_fftw3=no;;
                      *)    AC_MSG_ERROR([bad value ${enableval} for --disable-fftw3]);;
                esac],
              [enable_fftw3=yes])

if test x${enable_fftw3} = xyes; then
	PKG_CHECK_MODULES([fftw3], fftw3)
	AC_SUBST(fftw3_LIBS)
	AC_SUBST(fftw3_CFLAGS)
	AC_DEFINE(ENABLE_FFTW3, 1, [Enable Ledmatrix mpd fft support])

fi
AM_CONDITIONAL(ENABLE_FFTW3, test x$enable_fftw3 = xyes)


AC_OUTPUT(Makefile)

if test x${enable_fftw3} = xyes; then
	echo "Use fftw3:  enabled"
else
	echo "Use fftw3:  disabled"
fi

