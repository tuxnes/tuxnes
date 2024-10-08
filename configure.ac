# SPDX-FileCopyrightText: Authors of TuxNES
# SPDX-License-Identifier: GPL-2.0-or-later

#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([TuxNES],[0.75],[https://github.com/tuxnes/tuxnes/issues])
AC_CONFIG_SRCDIR([src/consts.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_SILENT_RULES([yes])

# Checks for programs.
PKG_PROG_PKG_CONFIG
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_AS
AC_PROG_RANLIB
AC_PROG_INSTALL

## Check whether user wants simple warnings or advanced warnings
WARNING_CFLAGS="-Wall -Wextra -pedantic -Wbad-function-cast \
-Wcast-align -Wcast-qual -Wchar-subscripts -Wconversion -Winline \
-Wmissing-prototypes -Wnested-externs -Wpointer-arith \
-Wredundant-decls -Wshadow -Wstrict-prototypes -Wwrite-strings"
AC_ARG_ENABLE([warnings],
	[AS_HELP_STRING([--enable-warnings],
		[enable more compilation warning checks])],
	[],
	[enable_warnings="no"])
AS_IF([test "x$enable_warnings" = "xno"],[WARNING_CFLAGS=""])
AC_SUBST([WARNING_CFLAGS])

## Check whether user wants to enable profiling support
PROFILING_CFLAGS="-pg"
AC_ARG_ENABLE([profiling],
	[AS_HELP_STRING([--enable-profiling],
		[enable profiling of functions])],
	[],
	[enable_profiling="no"])
AS_IF([test "x$enable_profiling" = "xno"],[PROFILING_CFLAGS=""])
AC_SUBST([PROFILING_CFLAGS])

# Checks for libraries.

# Checks for header files.
AC_PATH_XTRA
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stddef.h sys/ioctl.h sys/time.h unistd.h])
AC_CHECK_HEADERS([linux/joystick.h linux/soundcard.h sys/soundcard.h])
AC_CHECK_HEADERS([machine/endian.h sys/endian.h endian.h],[break])
AC_CHECK_HEADERS([getopt.h])

AS_IF([test "x$no_x" != "xyes"],[
	AC_CHECK_HEADERS([X11/xpm.h X11/vroot.h sys/ipc.h sys/shm.h])
	AC_CHECK_HEADERS([X11/extensions/XShm.h X11/extensions/Xext.h X11/extensions/scrnsaver.h X11/extensions/Xrender.h],[],[],[[#include <X11/Xlib.h>]])
])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_FUNC_STRTOD
AC_CHECK_FUNCS([atexit ftruncate gettimeofday memmove memset strcasecmp strrchr strtoul])

AC_CHECK_LIB([m],[sin])

PKG_CHECK_MODULES([DEFLATE], [zlib minizip],
	[AS_VAR_SET([have_deflate], [yes])],
	[AS_VAR_SET([have_deflate], [no])])
AS_IF([test "x$have_deflate" = "xyes"],[
	# Ugh, existing usage is conflated to mean both -lz and -lminizip
	AC_DEFINE([HAVE_LIBZ], [1], [Define to 1 if you have the `z' library (-lz).])
])

PKG_CHECK_MODULES([HQX], [hqx],
	[AS_VAR_SET([have_hqx], [yes])],
	[AS_VAR_SET([have_hqx], [no])])
AM_CONDITIONAL([BUILD_HQX], [test "x$have_hqx" = "xno"])
AS_IF([test "x$have_hqx" = "xno"],[
	AC_SUBST([HQX_CFLAGS], ['-I$(top_srcdir)/submodules/hqx/src'])
	AC_SUBST([HQX_LIBS], ['$(top_builddir)/libhqx.a'])
])

AS_IF([test "x$no_x" != "xyes"],[
	save_CFLAGS="$CFLAGS"
	save_LIBS="$LIBS"
	CFLAGS="$CFLAGS $X_CFLAGS"
	LIBS="$X_PRE_LIBS -lX11 $X_EXTRA_LIBS $X_LIBS"
	AC_CHECK_LIB([Xpm],[XpmWriteFileFromImage])
	AC_CHECK_LIB([Xext],[XMissingExtension])
	AC_CHECK_LIB([Xss],[XScreenSaverSuspend])
	AC_CHECK_LIB([Xrender],[XRenderCreatePicture])
	X_LIBS="$LIBS"
	CFLAGS="$save_CFLAGS"
	LIBS="$save_LIBS"
])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_CONFIG_HEADERS([src/config.h])
AC_OUTPUT
