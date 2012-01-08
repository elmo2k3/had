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
AC_CHECK_HEADERS([stdlib.h string.h unistd.h inet.h netdb.h])

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

AC_ARG_ENABLE(libcrypto,
              [  --disable-libcrypto			Support for last.fm scrobbler],
              [ case "${enableval}" in
                    yes)    enable_libcrypto=yes;;
                     no)    enable_libcrypto=no;;
                      *)    AC_MSG_ERROR([bad value ${enableval} for --disable-libcrypto]);;
                esac],
              [enable_libcrypto=yes])

if test x${enable_libcrypto} = xyes; then
	PKG_CHECK_MODULES([libcrypto], libcrypto >= 0.9.8m)
	AC_SUBST(libcrypto_LIBS)
	AC_SUBST(libcrypto_CFLAGS)
	AC_DEFINE(ENABLE_LIBCRYPTO, 1, [Enable scrobbler])

fi
AM_CONDITIONAL(ENABLE_LIBCRYPTO, test x$enable_libcrypto = xyes)

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

AC_ARG_ENABLE(libmpd,
              [  --disable-libmpd			Support for mpd],
              [ case "${enableval}" in
                    yes)    enable_libmpd=yes;;
                     no)    enable_libmpd=no;;
                      *)    AC_MSG_ERROR([bad value ${enableval} for --disable-libmpd]);;
                esac],
              [enable_libmpd=yes])

if test x${enable_libmpd} = xyes; then
	PKG_CHECK_MODULES([libmpd], libmpd >= 0.19.0)
	AC_SUBST(libmpd_LIBS)
	AC_SUBST(libmpd_CFLAGS)
	AC_DEFINE(ENABLE_LIBMPD, 1, [Enable mpd connection])

fi
AM_CONDITIONAL(ENABLE_LIBMPD, test x$enable_libmpd = xyes)

AC_ARG_ENABLE(libcurl,
              [  --disable-libcurl			Support for sipgate sms],
              [ case "${enableval}" in
                    yes)    enable_libcurl=yes;;
                     no)    enable_libcurl=no;;
                      *)    AC_MSG_ERROR([bad value ${enableval} for --disable-libcurl]);;
                esac],
              [enable_libcurl=yes])

if test x${enable_libcurl} = xyes; then
	PKG_CHECK_MODULES([libcurl], libcurl >= 7.0.0)
	AC_SUBST(libcurl_LIBS)
	AC_SUBST(libcurl_CFLAGS)
	AC_DEFINE(ENABLE_LIBCURL, 1, [Enable sipgate sms support])

fi
AM_CONDITIONAL(ENABLE_LIBCURL, test x$enable_libcurl = xyes)


AC_OUTPUT(Makefile)

if test x${enable_fftw3} = xyes; then
	echo "Use fftw3:  enabled"
else
	echo "Use fftw3:  disabled"
fi
if test x${enable_libmpd} = xyes; then
	echo "Use libmpd:  enabled"
else
	echo "Use libmpd:  disabled"
fi
if test x${enable_libcurl} = xyes; then
	echo "Use libcurl:  enabled"
else
	echo "Use libcurl:  disabled"
fi
if test x${enable_libcrypto} = xyes; then
	echo "Use libcrypto:  enabled"
else
	echo "Use libcrypto:  disabled"
fi

