AUTOMAKE_OPTIONS = subdir-objects

AM_CFLAGS = $(WARNING_CFLAGS) $(PROFILING_CFLAGS)
AM_CFLAGS += -I../submodules/hqx/src
AM_CPPFLAGS = $(DEFLATE_CFLAGS) $(X_CFLAGS)

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
	x11.c \
	../submodules/hqx/src/init.c \
	../submodules/hqx/src/hq2x.c \
	../submodules/hqx/src/hq3x.c \
	../submodules/hqx/src/hq4x.c

tuxnes_LDADD = table.o $(DEFLATE_LIBS) $(X_LIBS)
tuxnes_LDFLAGS = $(AM_LDFLAGS) -Wl,-z,noexecstack

comptbl_SOURCES = comptbl.c

EXTRA_DIST = table.x86 romfixer
CLEANFILES = compdata

compdata: comptbl $(srcdir)/table.x86
	./comptbl < $(srcdir)/table.x86

table.o: compdata
	$(LD) -r -b binary -r -defsym TRANS_TBL=_binary_compdata_start $< -o $@
