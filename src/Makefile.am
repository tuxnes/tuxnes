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
tuxnes_LDADD = table.o $(DEFLATE_LIBS) $(HQX_LIBS) $(X_LIBS)
tuxnes_LDFLAGS = $(AM_LDFLAGS) -Wl,-z,noexecstack

comptbl_SOURCES = comptbl.c

EXTRA_DIST = table.x86 romfixer
CLEANFILES = compdata

compdata: comptbl $(srcdir)/table.x86
	./comptbl < $(srcdir)/table.x86

table.o: compdata
	$(LD) -r -b binary -r -defsym TRANS_TBL=_binary_compdata_start $< -o $@
