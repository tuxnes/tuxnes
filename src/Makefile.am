# SPDX-FileCopyrightText: Authors of TuxNES
# SPDX-License-Identifier: GPL-2.0-or-later

AM_CFLAGS = $(WARNING_CFLAGS) $(PROFILING_CFLAGS)

bin_PROGRAMS = tuxnes
bin_SCRIPTS = romfixer
noinst_PROGRAMS = comptbl

tuxnes_SOURCES = \
	consts.h \
	globals.h \
	emu.c \
	x86.S \
	d6502.c \
	dynrec.c \
	io.c \
	fb.c \
	pixels.h \
	gamegenie.c gamegenie.h \
	controller.c controller.h \
	joystick.c joystick.h \
	loader.c loader.h \
	mapper.c mapper.h \
	ntsc_pal.c \
	sound.c sound.h \
	renderer.c renderer.h \
	screenshot.c screenshot.h \
	x11.c

tuxnes_CPPFLAGS = $(AM_CPPFLAGS) $(DEFLATE_CFLAGS) $(HQX_CFLAGS) $(X_CFLAGS)
tuxnes_LDADD = $(DEFLATE_LIBS) $(HQX_LIBS) $(X_LIBS)

comptbl_SOURCES = comptbl.c

EXTRA_DIST = table.x86 romfixer
CLEANFILES = compdata

tuxnes-x86.$(OBJEXT): compdata
compdata: comptbl $(srcdir)/table.x86
	$(AM_V_GEN)./comptbl < $(srcdir)/table.x86
