#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(had, 0.3.0, bjoern@bjoern-b.de)
AM_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE(had, 0.3.0)
AM_SILENT_RULES([no])

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

PKG_PROG_PKG_CONFIG

AX_LIB_POSTGRESQL(8.0)
AM_GLIB_GNU_GETTEXT



#glib
PKG_CHECK_MODULES([glib], glib-2.0 >= 2.10)
AC_SUBST(glib_LIBS)
AC_SUBST(glib_CFLAGS)

PKG_CHECK_MODULES([gthread], gthread-2.0 >= 2.10)
AC_SUBST(gthread_LIBS)
AC_SUBST(gthread_CFLAGS)

AC_ARG_ENABLE(ipv6,
	AS_HELP_STRING([--disable-ipv6],
		[disable IPv6 support (default: enable)]),,
	[enable_ipv6=yes])

if test x$enable_ipv6 = xyes; then
	AC_MSG_CHECKING(for ipv6)
	AC_EGREP_CPP([AP_maGiC_VALUE],
	[
#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#include <netdb.h>
#ifdef PF_INET6
#ifdef AF_INET6
AP_maGiC_VALUE
#endif
#endif
	],
	AC_DEFINE(HAVE_IPV6, 1, [Define if IPv6 support present])
	AC_MSG_RESULT([yes]),
	AC_MSG_RESULT([no])
)
fi

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

MYSQL_PREFIX=/usr
AC_ARG_WITH(libmysqlclient,
              [  --with-libmysqlclient=<path>		Support for mysql],
			  [MYSQL_PREFIX=$with_libmysqlclient],
			  AC_MSG_WARN([You can call configure with the --with-mysql option.
				This tells configure where to find the MySql C library and headers if configure cannot
				locate them automatically.
  				e.g. --with-mysql=/usr/local or --with-mysql=/usr])
)

#enable_libmysqlclient=yes
#if test x${enable_libmysqlclient} = xyes; then
	AC_SUBST(MYSQL_PREFIX)
	MYSQL_LIBS="-L${MYSQL_PREFIX}/lib/mysql -L${MYSQL_PREFIX}/lib64/mysql -lmysqlclient -lz"
	MYSQL_CFLAGS="-I${MYSQL_PREFIX}/include"
	AC_SUBST(MYSQL_LIBS)
	AC_SUBST(MYSQL_CFLAGS)
	AC_DEFINE(ENABLE_LIBMYSQLCLIENT, 1, [Enable mysql support])

#fi

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
if test x${enable_libmysqlclient} = xyes; then
	echo "Use libmysqlclient:  enabled"
else
	echo "Use libmysqlclient:  disabled"
fi

