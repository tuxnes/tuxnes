# SPDX-FileCopyrightText: Authors of TuxNES
# SPDX-License-Identifier: GPL-2.0-or-later

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = . src
EXTRA_DIST = THANKS tuxnes.xpm tuxnes2.xpm BUGS CHANGES

# HQX especially needs -O3 for vectorization.
# Overwrite the user-specified CFLAGS (default is generally -g -O2)
# which has priority over AM_CFLAGS or any target-specific CFLAGS.
CFLAGS = -O3

if BUILD_HQX
noinst_LIBRARIES = libhqx.a

libhqx_a_SOURCES = \
	submodules/hqx/src/init.c \
	submodules/hqx/src/hq2x.c \
	submodules/hqx/src/hq3x.c \
	submodules/hqx/src/hq4x.c \
	submodules/hqx/src/hqx.h \
	submodules/hqx/src/common.h
endif
