#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([TuxNES],[0.75],[tuxnes-dev@dod.hpi.net])
AC_CONFIG_SRCDIR([src/consts.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([src/config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_AS
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
AC_ARG_WITH([ggi],
	[AS_HELP_STRING([--without-ggi],
		[disable check for GGI (General Graphics Interface)])],
	[],
	[with_ggi="yes"])

AC_ARG_WITH([w],
	[AS_HELP_STRING([--without-w],
		[disable check for the W Window System])],
	[],
	[with_w="yes"])

# Checks for header files.
AC_PATH_XTRA
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS([fcntl.h stddef.h stdlib.h string.h sys/ioctl.h sys/time.h unistd.h])
AC_CHECK_HEADERS([features.h linux/joystick.h linux/soundcard.h sys/soundcard.h])
AC_CHECK_HEADERS([getopt.h])
AC_CHECK_HEADERS([ppm.h])

AS_IF([test "x$with_ggi" = "xyes"],[
	AC_CHECK_HEADERS([ggi/gii.h ggi/ggi.h])
])

AS_IF([test "x$with_w" = "xyes"],[
	AC_CHECK_HEADERS([Wlib.h])
])

AS_IF([test "x$no_x" != "xyes"],[
	AC_CHECK_HEADERS([X11/xpm.h X11/vroot.h sys/ipc.h sys/shm.h])
	AC_CHECK_HEADERS([X11/extensions/XShm.h X11/extensions/Xext.h],[],[],[[#include <X11/Xlib.h>]])
])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
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
AC_CHECK_LIB([z],[gzgetc])

AC_CHECK_LIB([pbm],[pbm_writepbm])
AC_CHECK_LIB([pgm],[pgm_writepgm])
AC_CHECK_LIB([ppm],[ppm_writeppm])

GGI_LIBS=""
AS_IF([test "x$with_ggi" = "xyes"],[
	save_LIBS="$LIBS"
	LIBS=""
	AC_CHECK_LIB([ggi],[ggiInit])
	AC_CHECK_LIB([gii],[giiInit])
	GGI_LIBS="$LIBS"
	LIBS="$save_LIBS"
])
AC_SUBST([GGI_LIBS])

W_LIBS=""
AS_IF([test "x$with_w" = "xyes"],[
	save_LIBS="$LIBS"
	LIBS=""
	AC_CHECK_LIB([W],[w_init])
	W_LIBS="$LIBS"
	LIBS="$save_LIBS"
])
AC_SUBST([W_LIBS])

AS_IF([test "x$no_x" != "xyes"],[
	save_CFLAGS="$CFLAGS"
	save_LIBS="$LIBS"
	CFLAGS="$CFLAGS $X_CFLAGS"
	LIBS="$X_PRE_LIBS -lX11 $X_EXTRA_LIBS $X_LIBS"
	AC_CHECK_LIB([Xpm],[XpmWriteFileFromImage])
	AC_CHECK_LIB([Xext],[XMissingExtension])
	X_LIBS="$LIBS"
	CFLAGS="$save_CFLAGS"
	LIBS="$save_LIBS"
])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
