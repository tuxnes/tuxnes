AM_CFLAGS = $(WARNING_CFLAGS) $(PROFILING_CFLAGS) $(X_CFLAGS)

bin_PROGRAMS = tuxnes
bin_SCRIPTS = romfixer
noinst_PROGRAMS = comptbl

tuxnes_SOURCES = x86.S d6502.c dynrec.c emu.c fb.c gamegenie.c io.c mapper.c \
	sound.c sound.h ggi.c x11.c w.c consts.h gamegenie.h globals.h \
	mapper.h renderer.c renderer.h ntsc_pal.c unzip.c unzip.h \
	ziploader.c ziploader.h
EXTRA_tuxnes_SOURCES = pixels.h
tuxnes_LDADD = table.o $(W_LIBS) $(GGI_LIBS) $(X_LIBS)

comptbl_SOURCES = comptbl.c

EXTRA_DIST = table.x86 THANKS romfixer tuxnes.xpm tuxnes2.xpm BUGS CHANGES INSTALL.BSD
CLEANFILES = compdata

compdata: comptbl table.x86
	./comptbl

table.o: compdata
	ld -r -b binary -r -defsym TRANS_TBL=_binary_compdata_start $< -o table.o
