/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * $Id: emu.c,v 1.79 2001/04/11 21:45:47 tmmm Exp $
 *
 * Description: This file contains the main() function of the emulator.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif /* HAVE_FEATRES_H */

#include <ctype.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#ifdef HAVE_LINUX_JOYSTICK_H
#include <linux/joystick.h>
#endif /* HAVE_LINUX_JOYSTICK_H */
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_LIBZ
#include <zlib.h>
#include "unzip.h"
#include "ziploader.h"
#endif

#include "consts.h"
#include "gamegenie.h"
#include "globals.h"
#include "mapper.h"
#include "renderer.h"
#include "sound.h"

u_char	*ROM_BASE;
u_char	*VROM_BASE;
char	*filename;
const char	*homedir;
char	 savefile[1024];
char	*tuxnesdir;                /* buffer for $HOME/.tuxnes dir */
char    *basefilename;  /* base filename without the extensions */
unsigned int	ROM_PAGES;
unsigned int	VROM_PAGES;
unsigned int    ROM_MASK;
unsigned int    VROM_MASK;
unsigned int    VROM_MASK_1k;
const char *sample_format_name = "8";
int	dirtyheader = 0;
int	disassemble = 0;
int	dolink = 0;
int	MAPPERNUMBER = -1;
int	SRAM_ENABLED;
int	gamegenie = 0;
int	irqflag = 0;
int	mapmirror = 0;
int	mapperoverride = 0;
int     ignorebadinstr = 0;
int	showheader = 0;
int	unisystem = 0;
int	verbose = 0;

/* joystick variables */
unsigned char	jsaxes[2] = {2, 2};
unsigned char	jsbuttons[2] = {2, 2};
int	 jsfd[2] = {-1, -1};
int	 jsversion[2] = {0x000800, 0x000800};
const char	*jsdevice[2] = {0, 0};
const char	*rendname = "auto";

#define JS_DEFAULT_MAP \
{ { BUTTONB, SELECTBUTTON, BUTTONA, STARTBUTTON, \
    BUTTONA, BUTTONB, SELECTBUTTON, STARTBUTTON, \
    PAUSEDISPLAY, 0, 0, 0,  0, 0, 0, 0, \
    0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0 }, \
  { { LEFT, RIGHT }, { UP, DOWN }, { 0, 0 }, { 0, 0 }, \
    { LEFT, RIGHT }, { UP, DOWN }, { 0, 0 }, { 0, 0 } } }
struct js_nesmap js_nesmaps[2] = { JS_DEFAULT_MAP, JS_DEFAULT_MAP };
#undef JS_DEFAULT_MAP
unsigned char js_mapsequence[JS_MAX_NES_BUTTONS] = 
{       BUTTONA, BUTTONB, STARTBUTTON, SELECTBUTTON,
	LEFT, RIGHT, UP, DOWN, PAUSEDISPLAY };
unsigned char js_axismeso[JS_MAX_AXES] = { 0, 0, 0, 0, 0, 0, 0, 0 };
unsigned char js_usermapped2button[2][2] = {{0, 0}, {0, 0}};

#define USAGE "Usage: %s [--help] [options] filename\n"

static void	help_help(int);
static void	help_version(int);
static void	help_options(int);
static void	help_controls(int);
static void	help_synonyms(int);
static void	help_palettes(int);
static void	help_sound(int);

/* help variables */
const struct {
  int is_terse;
  const char *name, *desc;
  void (*dfn)(int);
} topics[] = {
  /* The interactive default help topic is the first in the array */
  { 1,
    "help",
    "Help topics",
    help_help },
  { 1,
    "options",
    "Major command-line options",
    help_options },
  { 1,
    "sound",
    "Sound synthesis options",
    help_sound },
  { 1,
    "palettes",
    "Custom palette options",
    help_palettes },
  { 0,
    "synonyms",
    "Alternate option names",
    help_synonyms },
  { 0,
    "controls",
    "Keyboard and joystick options and bindings",
    help_controls },
  { 0,
    "version",
    "Version and copyright information",
    help_version },
  /* The non-interactive default help topic is the last in the array */
  { 1, "all",
    "All help topics",
    (void (*)(int))0 }
};
static void	help(const char *progname, const char *topic);

#ifdef HAVE_LIBM
extern unsigned int *ntsc_palette(double hue, double tint);
double hue = 332.0;
double tint = 0.5;
#endif /* HAVE_LIBM */

static void	js_set_nesmaps(char *);
static void	loadpal(char *);
static void	restoresavedgame(void);
static void	traphandler(int);

void	InitMapperSubsystem(void);
char	MapperName[MAXMAPPER + 1][30];
void	quit(void);
void	START(void);
void	translate(int);

/* color palettes */
struct
  {
    const char *name, *desc;
    unsigned int data[64];
  }
palettes[] =
{
  /* The default NES palette must be the first entry in the array */
	{ "loopy", "Loopy's NES palette",
	{ 0x757575, 0x271b8f, 0x0000ab, 0x47009f,
	  0x8f0077, 0xab0013, 0xa70000, 0x7f0b00,
	  0x432f00, 0x004700, 0x005100, 0x003f17,
	  0x1b3f5f, 0x000000, 0x000000, 0x000000,
	  0xbcbcbc, 0x0073ef, 0x233bef, 0x8300f3,
	  0xbf00bf, 0xe7005b, 0xdb2b00, 0xcb4f0f,
	  0x8b7300, 0x009700, 0x00ab00, 0x00933b,
	  0x00838b, 0x000000, 0x000000, 0x000000,
	  0xffffff, 0x3fbfff, 0x5f97ff, 0xa78bfd,
	  0xf77bff, 0xff77b7, 0xff7763, 0xff9b3b,
	  0xf3bf3f, 0x83d313, 0x4fdf4b, 0x58f898,
	  0x00ebdb, 0x000000, 0x000000, 0x000000,
	  0xffffff, 0xabe7ff, 0xc7d7ff, 0xd7cbff,
	  0xffc7ff, 0xffc7db, 0xffbfb3, 0xffdbab,
	  0xffe7a3, 0xe3ffa3, 0xabf3bf, 0xb3ffcf,
	  0x9ffff3, 0x000000, 0x000000, 0x000000 }
	},
	{ "quor", "Quor's palette from Nestra 0.63",
	{ 0x3f3f3f, 0x001f3f, 0x00003f, 0x1f003f,
	  0x3f003f, 0x3f0020, 0x3f0000, 0x3f2000,
	  0x3f3f00, 0x203f00, 0x003f00, 0x003f20,
	  0x003f3f, 0x000000, 0x000000, 0x000000,
	  0x7f7f7f, 0x405f7f, 0x40407f, 0x5f407f,
	  0x7f407f, 0x7f4060, 0x7f4040, 0x7f6040,
	  0x7f7f40, 0x607f40, 0x407f40, 0x407f60,
	  0x407f7f, 0x000000, 0x000000, 0x000000,
	  0xbfbfbf, 0x809fbf, 0x8080bf, 0x9f80bf,
	  0xbf80bf, 0xbf80a0, 0xbf8080, 0xbfa080,
	  0xbfbf80, 0xa0bf80, 0x80bf80, 0x80bfa0,
	  0x80bfbf, 0x000000, 0x000000, 0x000000,
	  0xffffff, 0xc0dfff, 0xc0c0ff, 0xdfc0ff,
	  0xffc0ff, 0xffc0e0, 0xffc0c0, 0xffe0c0,
	  0xffffc0, 0xe0ffc0, 0xc0ffc0, 0xc0ffe0,
	  0xc0ffff, 0x000000, 0x000000, 0x000000 }
	},
	{ "chris", "Chris Covell's NES palette",
	{ 0x808080, 0x003DA6, 0x0012B0, 0x440096,
	  0xA1005E, 0xC70028, 0xBA0600, 0x8C1700,
	  0x5C2F00, 0x104500, 0x054A00, 0x00472E,
	  0x004166, 0x000000, 0x050505, 0x050505,
	  0xC7C7C7, 0x0077FF, 0x2155FF, 0x8237FA,
	  0xEB2FB5, 0xFF2950, 0xFF2200, 0xD63200,
	  0xC46200, 0x358000, 0x058F00, 0x008A55,
	  0x0099CC, 0x212121, 0x090909, 0x090909,
	  0xFFFFFF, 0x0FD7FF, 0x69A2FF, 0xD480FF,
	  0xFF45F3, 0xFF618B, 0xFF8833, 0xFF9C12,
	  0xFABC20, 0x9FE30E, 0x2BF035, 0x0CF0A4,
	  0x05FBFF, 0x5E5E5E, 0x0D0D0D, 0x0D0D0D,
	  0xFFFFFF, 0xA6FCFF, 0xB3ECFF, 0xDAABEB,
	  0xFFA8F9, 0xFFABB3, 0xFFD2B0, 0xFFEFA6,
	  0xFFF79C, 0xD7E895, 0xA6EDAF, 0xA2F2DA,
	  0x99FFFC, 0xDDDDDD, 0x111111, 0x111111 }
	},
	{ "matt", "Matthew Conte's NES palette",
	{ 0x808080, 0x0000bb, 0x3700bf, 0x8400a6,
	  0xbb006a, 0xb7001e, 0xb30000, 0x912600,
	  0x7b2b00, 0x003e00, 0x00480d, 0x003c22,
	  0x002f66, 0x000000, 0x050505, 0x050505,
	  0xc8c8c8, 0x0059ff, 0x443cff, 0xb733cc,
	  0xff33aa, 0xff375e, 0xff371a, 0xd54b00,
	  0xc46200, 0x3c7b00, 0x1e8415, 0x009566,
	  0x0084c4, 0x111111, 0x090909, 0x090909,
	  0xffffff, 0x0095ff, 0x6f84ff, 0xd56fff,
	  0xff77cc, 0xff6f99, 0xff7b59, 0xff915f,
	  0xffa233, 0xa6bf00, 0x51d96a, 0x4dd5ae,
	  0x00d9ff, 0x666666, 0x0d0d0d, 0x0d0d0d,
	  0xffffff, 0x84bfff, 0xbbbbff, 0xd0bbff,
	  0xffbfea, 0xffbfcc, 0xffc4b7, 0xffccae,
	  0xffd9a2, 0xcce199, 0xaeeeb7, 0xaaf7ee,
	  0xb3eeff, 0xdddddd, 0x111111, 0x111111 }
	},
	{ "pasofami", "Palette from PasoFami/99",
	{ 0x7f7f7f, 0x0000ff, 0x0000bf, 0x472bbf,
	  0x970087, 0xab0023, 0xab1300, 0x8b1700,
	  0x533000, 0x007800, 0x006b00, 0x005b00,
	  0x004358, 0x000000, 0x000000, 0x000000,
	  0xbfbfbf, 0x0078f8, 0x0058f8, 0x6b47ff,
	  0xdb00cd, 0xe7005b, 0xf83800, 0xe75f13,
	  0xaf7f00, 0x00b800, 0x00ab00, 0x00ab47,
	  0x008b8b, 0x000000, 0x000000, 0x000000,
	  0xf8f8f8, 0x3fbfff, 0x6b88ff, 0x9878f8,
	  0xf878f8, 0xf85898, 0xf87858, 0xffa347,
	  0xf8b800, 0xb8f818, 0x5bdb57, 0x58f898,
	  0x00ebdb, 0x787878, 0x000000, 0x000000,
	  0xffffff, 0xa7e7ff, 0xb8b8f8, 0xd8b8f8,
	  0xf8b8f8, 0xfba7c3, 0xf0d0b0, 0xffe3ab,
	  0xfbdb7b, 0xd8f878, 0xb8f8b8, 0xb8f8d8,
	  0x00ffff, 0xf8d8f8, 0x000000, 0x000000 }
	},
	{ "crashman", "CrashMan's NES palette",
	{ 0x585858, 0x001173, 0x000062, 0x472bbf,
	  0x970087, 0x910009, 0x6f1100, 0x4c1008,
	  0x371e00, 0x002f00, 0x005500, 0x004d15,
	  0x002840, 0x000000, 0x000000, 0x000000,
	  0xa0a0a0, 0x004499, 0x2c2cc8, 0x590daa,
	  0xae006a, 0xb00040, 0xb83418, 0x983010,
	  0x704000, 0x308000, 0x207808, 0x007b33,
	  0x1c6888, 0x000000, 0x000000, 0x000000,
	  0xf8f8f8, 0x267be1, 0x5870f0, 0x9878f8,
	  0xff73c8, 0xf060a8, 0xd07b37, 0xe09040,
	  0xf8b300, 0x8cbc00, 0x40a858, 0x58f898,
	  0x00b7bf, 0x787878, 0x000000, 0x000000,
	  0xffffff, 0xa7e7ff, 0xb8b8f8, 0xd8b8f8,
	  0xe6a6ff, 0xf29dc4, 0xf0c0b0, 0xfce4b0,
	  0xe0e01e, 0xd8f878, 0xc0e890, 0x95f7c8,
	  0x98e0e8, 0xf8d8f8, 0x000000, 0x000000 }
	},
	{ "mess", "palette from the MESS NES driver",
	{ 0x747474, 0x24188c, 0x0000a8, 0x44009c,
	  0x8c0074, 0xa80010, 0xa40000, 0x7c0800,
	  0x402c00, 0x004400, 0x005000, 0x003c14,
	  0x183c5c, 0x000000, 0x000000, 0x000000,
	  0xbcbcbc, 0x0070ec, 0x2038ec, 0x8000f0,
	  0xbc00bc, 0xe40058, 0xd82800, 0xc84c0c,
	  0x887000, 0x009400, 0x00a800, 0x009038,
	  0x008088, 0x000000, 0x000000, 0x000000,
	  0xfcfcfc, 0x3cbcfc, 0x5c94fc, 0x4088fc,
	  0xf478fc, 0xfc74b4, 0xfc7460, 0xfc9838,
	  0xf0bc3c, 0x80d010, 0x4cdc48, 0x58f898,
	  0x00e8d8, 0x000000, 0x000000, 0x000000,
	  0xfcfcfc, 0xa8e4fc, 0xc4d4fc, 0xd4c8fc,
	  0xfcc4fc, 0xfcc4d8, 0xfcbcb0, 0xfcd8a8,
	  0xfce4a0, 0xe0fca0, 0xa8f0bc, 0xb0fccc,
	  0x9cfcf0, 0x000000, 0x000000, 0x000000 }
	},
	{ "zaphod-cv", "Zaphod's VS Castlevania palette",
	{ 0x7f7f7f, 0xffa347, 0x0000bf, 0x472bbf,
	  0x970087, 0xf85898, 0xab1300, 0xf8b8f8,
	  0xbf0000, 0x007800, 0x006b00, 0x005b00,
	  0xffffff, 0x9878f8, 0x000000, 0x000000,
	  0xbfbfbf, 0x0078f8, 0xab1300, 0x6b47ff,
	  0x00ae00, 0xe7005b, 0xf83800, 0x7777ff,
	  0xaf7f00, 0x00b800, 0x00ab00, 0x00ab47,
	  0x008b8b, 0x000000, 0x000000, 0x472bbf,
	  0xf8f8f8, 0xffe3ab, 0xf87858, 0x9878f8,
	  0x0078f8, 0xf85898, 0xbfbfbf, 0xffa347,
	  0xc800c8, 0xb8f818, 0x7f7f7f, 0x007800,
	  0x00ebdb, 0x000000, 0x000000, 0xffffff,
	  0xffffff, 0xa7e7ff, 0x5bdb57, 0xe75f13,
	  0x004358, 0x0000ff, 0xe7005b, 0x00b800,
	  0xfbdb7b, 0xd8f878, 0x8b1700, 0xffe3ab,
	  0x00ffff, 0xab0023, 0x000000, 0x000000 }
	},
	{ "zaphod-smb", "Zaphod's VS SMB palette",
	{ 0x626a00, 0x0000ff, 0x006a77, 0x472bbf,
	  0x970087, 0xab0023, 0xab1300, 0xb74800,
	  0xa2a2a2, 0x007800, 0x006b00, 0x005b00,
	  0xffd599, 0xffff00, 0x009900, 0x000000,
	  0xff66ff, 0x0078f8, 0x0058f8, 0x6b47ff,
	  0x000000, 0xe7005b, 0xf83800, 0xe75f13,
	  0xaf7f00, 0x00b800, 0x5173ff, 0x00ab47,
	  0x008b8b, 0x000000, 0x91ff88, 0x000088,
	  0xf8f8f8, 0x3fbfff, 0x6b0000, 0x4855f8,
	  0xf878f8, 0xf85898, 0x595958, 0xff009d,
	  0x002f2f, 0xb8f818, 0x5bdb57, 0x58f898,
	  0x00ebdb, 0x787878, 0x000000, 0x000000,
	  0xffffff, 0xa7e7ff, 0x590400, 0xbb0000,
	  0xf8b8f8, 0xfba7c3, 0xffffff, 0x00e3e1,
	  0xfbdb7b, 0xffae00, 0xb8f8b8, 0xb8f8d8,
	  0x00ff00, 0xf8d8f8, 0xffaaaa, 0x004000 }
	},
	{ "vs-drmar", "VS Dr. Mario palette",
	{ 0x5f97ff, 0x000000, 0x000000, 0x47009f,
	  0x00ab00, 0xffffff, 0xabe7ff, 0x000000,
	  0x000000, 0x000000, 0x000000, 0x000000,
	  0xe7005b, 0x000000, 0x000000, 0x000000,
	  0x5f97ff, 0x000000, 0x000000, 0x000000,
	  0x000000, 0x8b7300, 0xcb4f0f, 0x000000,
	  0xbcbcbc, 0x000000, 0x000000, 0x000000,
	  0x000000, 0x000000, 0x000000, 0x000000,
	  0x00ebdb, 0x000000, 0x000000, 0x000000,
	  0x000000, 0xff9b3b, 0x000000, 0x000000,
	  0x83d313, 0x000000, 0x3fbfff, 0x000000,
	  0x0073ef, 0x000000, 0x000000, 0x000000,
	  0x00ebdb, 0x000000, 0x000000, 0x000000,
	  0x000000, 0x000000, 0xf3bf3f, 0x000000,
	  0x005100, 0x000000, 0xc7d7ff, 0xffdbab,
	  0x000000, 0x000000, 0x000000, 0x000000 }
	},
	{ "vs-cv", "VS Castlevania palette",
	{ 0xaf7f00, 0xffa347, 0x008b8b, 0x472bbf,
	  0x970087, 0xf85898, 0xab1300, 0xf8b8f8,
	  0xf83800, 0x007800, 0x006b00, 0x005b00,
	  0xffffff, 0x9878f8, 0x00ab00, 0x000000,
	  0xbfbfbf, 0x0078f8, 0xab1300, 0x6b47ff,
	  0x000000, 0xe7005b, 0xf83800, 0x6b88ff,
	  0xaf7f00, 0x00b800, 0x6b88ff, 0x00ab47,
	  0x008b8b, 0x000000, 0x000000, 0x472bbf,
	  0xf8f8f8, 0xffe3ab, 0xf87858, 0x9878f8,
	  0x0078f8, 0xf85898, 0xbfbfbf, 0xffa347,
	  0x004358, 0xb8f818, 0x7f7f7f, 0x007800,
	  0x00ebdb, 0x000000, 0x000000, 0xffffff,
	  0xffffff, 0xa7e7ff, 0x5bdb57, 0x6b88ff,
	  0x004358, 0x0000ff, 0xe7005b, 0x00b800,
	  0xfbdb7b, 0xffa347, 0x8b1700, 0xffe3ab,
	  0xb8f818, 0xab0023, 0x000000, 0x007800 }
	},
/* The default VS palette must be the last entry in the array */
	{ "vs-smb", "VS SMB/VS Ice Climber palette",
	{ 0xaf7f00, 0x0000ff, 0x008b8b, 0x472bbf,
	  0x970087, 0xab0023, 0x0000ff, 0xe75f13,
	  0xbfbfbf, 0x007800, 0x5bdb57, 0x005b00,
	  0xf0d0b0, 0xffe3ab, 0x00ab00, 0x000000,
	  0xbfbfbf, 0x0078f8, 0x0058f8, 0x6b47ff,
	  0x000000, 0xe7005b, 0xf83800, 0xf87858,
	  0xaf7f00, 0x00b800, 0x6b88ff, 0x00ab47,
	  0x008b8b, 0x000000, 0x000000, 0x3fbfff,
	  0xf8f8f8, 0x006b00, 0x8b1700, 0x9878f8,
	  0x6b47ff, 0xf85898, 0x7f7f7f, 0xe7005b,
	  0x004358, 0xb8f818, 0x0078f8, 0x58f898,
	  0x00ebdb, 0xfbdb7b, 0x000000, 0x000000,
	  0xffffff, 0xa7e7ff, 0xb8b8f8, 0xf83800,
	  0xf8b8f8, 0xfba7c3, 0xffffff, 0x00ffff,
	  0xfbdb7b, 0xffa347, 0xb8f8b8, 0xb8f8d8,
	  0xb8f818, 0xf8d8f8, 0x000000, 0x007800 }
	}
};

unsigned char *palremap = 0, *paldata = 0;
unsigned int *NES_palette = 0;
int monochrome = 0;

void (*oldtraphandler)(int);

/****************************************************************************/

void
traphandler (int signum)
{
  if (signal (SIGTRAP, &traphandler) == SIG_ERR)
    {
      perror ("signal");
    }
  if (ignorebadinstr)
    return;
  /* this will only affect subsequently-compiled code */
  ignorebadinstr = 1;
  fprintf (stderr, "======================================================\n");
  fprintf (stderr, "Trap/Breakpoint: Unimplemented instruction detected\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "To ignore such instructions, run " PRETTY_NAME " with the\n");
  fprintf (stderr, "--ignore-unhandled option.\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "To find out which instruction caused the problem,\n");
  fprintf (stderr, "run " PRETTY_NAME " with the --disassemble option, and then\n");
  fprintf (stderr, "run GDB on the resulting core-file to find out which\n");
  fprintf (stderr, "recompiled instruction caused the problem. Please\n");
  fprintf (stderr, "send a description of the problem (and, if possible,\n");
  fprintf (stderr, "a fix) to <tuxnes-dev@dod.hpi.net>. Include this\n");
  fprintf (stderr, "information in your description:\n");
  fprintf (stderr, "\n");
  fprintf (stderr, PRETTY_NAME " release: %s-%s\n", PACKAGE, VERSION);
  fprintf (stderr, "Built on %s at %s\n", __DATE__, __TIME__);
  fprintf (stderr, "\n");
  fprintf (stderr, "OS version and platform:\n");
  fflush (stderr);
  system ("uname -a >&2");
  fprintf (stderr, "======================================================\n");
  fflush (stderr);
}


/****************************************************************************/

void
quit(void)
{
  int			fd;
  int			x;

  /* Clean up char pointers */
  free (tuxnesdir);
  free (basefilename);

  /* Save ram */
  for (x = 0; ((long long *) (RAM + 0x6000))[x] == 0; x++)
    if (x >= 1023)
      exit (0);                 /* Nothing to save */
  fd = open (savefile, O_CREAT | O_RDWR, 0666);
  if (fd > 0)
    {
      write (fd, RAM + 0x6000, 8192);
      close (fd);
      exit (0);
    }
  perror (savefile);
  exit (1);
}

/****************************************************************************/

static void
help(const char *progname, const char *topic)
{
  int			i;
  void (*dfn)(int) = (void (*)(int))0;
  const char *desc = 0;
  int dfns = 0;
  int len;
  int terse = 0;

  if (! topic)
    topic = "";
  len = strlen(topic);
  if ((! *topic) ||
      ((*topic == '-') &&
       ! topic[1])) {
    terse = 1;
    if (isatty (fileno (stdout)))
      topic = topics[0].name;
    else
      topic = topics[sizeof(topics)/sizeof(*topics) - 1].name;
  } else if (*topic == '-') {
    terse = 1;
    topic ++;
    len --;
  }
  for (i = 0; i < sizeof(topics)/sizeof(*topics); i++)
    if (terse ? topics[i].is_terse : 1)
      {
	if (!strcmp (topics[i].name, topic))
	  {
	    desc = topics[i].desc;
	    dfn = topics[i].dfn;
	    dfns = 1;
	    break;
	  }
	else if (!strncmp (topics[i].name, topic, len))
	  {
	    desc = topics[i].desc;
	    dfn = topics[i].dfn;
	    dfns++;
	}
      }
  if (dfns != 1) {
    if (dfns)
      fprintf (stderr, "%s: help topic name `%s' is ambiguous\n",
	       progname, topic);
    else
      fprintf (stderr, "%s: unrecognized help topic name `%s'\n",
	       progname, topic);
    fprintf (stderr, USAGE, progname);
    exit (2);
  }
  printf (USAGE, progname);
  if (dfn)
    {
      printf ("%s:\n", desc);
      dfn (terse);
    }
  else
    {
      for (i = 0; i < sizeof(topics)/sizeof(*topics); i++)
	if (topics[i].dfn && (terse ? topics[i].is_terse : 1))
	  {
	    printf ("%s:\n", topics[i].desc);
	    topics[i].dfn (terse);
	  }
    }
}

/****************************************************************************/

static void
help_help(int terse)
{
  int i;

  for (i = 0; i < sizeof(topics)/sizeof(*topics); i++)
    printf ("      --help=%-9s %s%s\n",
	    topics[i].name,
	    topics[i].desc,
	    (topics[i].dfn == help_help) ? " (this list)" : "");
  for (i = 0; i < sizeof(topics)/sizeof(*topics); i++)
    if (topics[i].is_terse)
      printf ("      --help=-%-8s Concise version of --help=%s\n",
	      topics[i].name,
	      topics[i].name);
}

/****************************************************************************/

static void
help_options(int terse)
{
     printf ("  -v, --verbose       Verbose output\n");
     printf ("  -f, --fix-mapper    Use only the low four bits of the mapper number\n");
     printf ("  -M, --mapper=...    Specify mapper to use\n");
     printf ("  -g, --gamegenie=... Game Genie code\n");
     printf ("  -H, --show-header   Show iNES header bytes\n");
     printf ("  -d, --disassemble   Disassemble\n");
     printf ("  -l, --link          Link branches optimization (may improve speed)\n");
     printf ("  -i, --ignore-unhandled\n");
     printf ("                      Ignore unhandled instructions (don't breakpoint)\n");
     printf ("  -m, --mirror=...    Manually specify type of mirroring\n");
     printf ("      h = Use horizontal mirroring\n");
     printf ("      v = Use vertical mirroring\n");
     printf ("      s = Use single-screen mirroring\n");
     printf ("      n = Use no mirroring\n");
     printf ("  -G, --geometry=WxH  Specify window/screen geometry\n");
     printf ("      --display=ID    Specify display/driver ID\n");
     printf ("  -E, --enlarge[=NUM] Enlarge by a factor of NUM (default: 2)\n");
     printf ("  -L, --scanlines[=%%] Set alternate scanline intensity (default: 0%%)\n");
     printf ("  -S, --static-color  Force static color allocation (prevents flicker)\n");
     printf ("  -r, --renderer=...  Select a rendering engine (default: auto)\n");
     for (renderer = renderers; renderer->name; renderer++)
	  printf ("      %-10s %s\n",
		  renderer->name,
		  renderer->fullname);
     printf ("  -I, --in-root       Display in root window\n");
     printf ("  -K, --sticky-keys   Hit keys once to press buttons, again to release\n");
     printf ("  -X, --swap-inputs   Swap P1 and P2 controls\n");
     if (terse)
     {
	  printf ("  -1, --js1[=FILE]    Use P1 joystick FILE (default: %s)\n", JS1);
	  printf ("  -2, --js2[=FILE]    Use P2 joystick FILE (default: %s)\n", JS2);
     }
}

/****************************************************************************/

static void
help_sound(int terse)
{
  printf ("  -s, --sound[=FILE]  Append sound data to FILE (default: %s)\n", DSP);
  printf ("      (specify sound file as mute or none for no sound, e.g., -smute)\n");
  printf ("  -F, --format=...    Use the specified sound sample format (default: %s)\n",
	  sample_format_name);
  for (sample_format = sample_formats; sample_format->name; sample_format++)
       printf ("      %-10s %s\n",
	       sample_format->name,
	       sample_format->fullname);
  printf ("  -R, --soundrate=NUM Set sound sample rate to NUM Hz (default: %d)\n",
	  sound_config.audiorate);
  printf ("  -D, --delay=NUM     Resynchronize if sound delay exceeds NUM seconds\n");
/*  printf ("  -e, --echo          Simulate echoing\n");
 */
}

/****************************************************************************/

static void
help_palettes(int terse)
{
  int i;

  printf ("  -p, --palfile=FILE  Load palette data from FILE\n");
  printf ("  -P, --palette=...   Use a specified built-in palette (default: %s)\n",
          palettes[0].name);
  for (i = 0; i < (sizeof (palettes) / sizeof (*palettes)); i++)
    printf ("      %-10s %s\n", palettes[i].name, palettes[i].desc);
  printf ("  -b, --bw            Convert palette to grayscale\n");
#ifdef HAVE_LIBM
  printf ("  -N, --ntsc-palette[=[HUE][,TINT]]\n"
	  "                      Use Kevin Horton's dynamic NTSC palette generator\n"
	  "                      with given hue angle (default: %g degrees) and tint\n"
	  "                      level (default: %g)\n", hue, tint);
#endif /* HAVE_LIBM */
}

/****************************************************************************/

static void
help_synonyms(int terse)
{
  printf ("  -c, --controls      Equivalent to --help=controls\n");
  printf ("  -h, --help          Equivalent to --help=-%s if stdout is a tty,\n",
	  topics[0].name);
  printf ("                      otherwise equivalent to --help=-%s\n",
	  topics[sizeof(topics)/sizeof(*topics) - 1].name);
  printf ("  -j, --joystick=FILE Equivalent to --js1=FILE\n");
  printf ("  -o, --old           Equivalent to --renderer=old\n");
  printf ("  -V, --version       Same information as --help=version\n");
}

/****************************************************************************/

static void
help_version(int terse)
{
  printf ("%s %s\n\n%s",
	  PACKAGE, VERSION,
	  "Copyright \xa9 1999-2001,  The TuxNES Team\n"
	  "\n"
	  "This program is free software; you can redistribute it and/or modify\n"
	  "it under the terms of the GNU General Public License as published by\n"
	  "the Free Software Foundation; either version 2 of the License, or\n"
	  "(at your option) any later version.\n"
	  "\n"
	  "This program is distributed in the hope that it will be useful,\n"
	  "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	  "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	  "GNU General Public License for more details.\n"
	  "\n"
	  "You should have received a copy of the GNU General Public License\n"
	  "along with this program; if not, write to the Free Software\n"
	  "Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n"
	  "\n"
          "Written by  The TuxNES Team; see the AUTHORS file.\n"
          "Based on Nestra v0.66 by Quor\n"
	  );
}

/****************************************************************************/

static void
help_controls(int terse)
{
  printf ("  -1, --js1[=FILE]            Use P1 joystick FILE (default: %s)\n", JS1);
  printf ("  -2, --js2[=FILE]            Use P2 joystick FILE (default: %s)\n", JS2);
  printf ("  -J, --joystick-map=MAPSPEC  Reassign joystick button bindings\n"
	  "    MAPSPEC:\n"
	  "      Specify joystick, then indicate buttons with 'B' and axes with 'A' in\n"
	  "      sequence:  A, B, Start, Select, Left, Right, Up, Down, Pause\n"
	  "      Multiple buttons may be bound to a single emulator control.\n"
	  "      Default bindings will be used if no buttons are specified.\n"
	  "      Examples:\n"
          "        1:B0,B1,B5,B2,A0,A1,B4\n" 
	  "        2:B0,B1,B5,B2,B10,B12,B11,B13,B4\n"
	  "        1:B0B8,B1B9,B5,B2\n"
	  "        2:,,,,A2,A3\n"
	  );
  printf ("\n");
  printf ("  Keyboard:\n");
  printf ("    Arrows      - Move (P1)\n");
  printf ("    A, C or Space\n");
  printf ("                - A button (P1)\n");
  printf ("    Z, X or D   - B button (P1)\n");
  printf ("    Tab         - Select button (P1)\n");
  printf ("    Enter       - Start button (P1)\n");
  printf ("    H, J, K, L  - Move (P2)\n");
  printf ("    B           - A button (P2)\n");
  printf ("    V           - B button (P2)\n");
  printf ("    F           - Select button (P2)\n");
  printf ("    G           - Start button (P2)\n");
  printf ("    BackSpace   - Reset game\n");
  printf ("    P, Pause    - Pause or resume emulation\n");
  printf ("    ESC         - Exit game\n");
  printf ("    0-8, `      - Adjust emulation speed\n");
  printf ("      0 stops the game, ` runs the game at half speed\n");
  printf ("      1 runs at normal speed, 2..8 runs at 2..8x normal speed\n");
  printf ("    S, F7, PrintScreen\n"
	  "                - Capture screenshot\n"
#ifdef HAVE_X
	  "                  Under X11, save to ~/.tuxnes/snap\?\?\?\?.xpm\n"
#endif /* HAVE_X */
#ifdef HAVE_GGI
	  "                  Under GGI, save to ~/.tuxnes/snap\?\?\?\?.ppm\n"
#endif /* HAVE_GGI */
#ifdef HAVE_W
	  "                  Under W, save to ~/.tuxnes/snap\?\?\?\?.p8m\n"
#endif /* HAVE_W */
       );
  printf ("                  (Note: XPM/PPM support must be installed and compiled\n");
  printf ("                   for this to work; see the README file for more info.)\n");
  printf ("\n");
  printf ("  Keypad:\n");
  printf ("    Arrows/12346789  - Move (P1)\n");
  printf ("    Space/Begin/5, Delete/Decimal\n");
  printf ("                     - A button (P1)\n");
  printf ("    Insert/0         - B button (P1)\n");
  printf ("    Add              - Select button (P1)\n");
  printf ("    Enter            - Start button (P1)\n");
  printf ("\n");
  printf ("  Joystick Axes:\n");
  printf ("    Axis 0           - Horizontal movement\n");
  printf ("    Axis 1           - Vertical movement\n");
  printf ("    Axis 2           - (unused)\n");
  printf ("    Axis 3           - (unused)\n");
  printf ("    Axis 4           - Horizontal movement\n");
  printf ("    Axis 5           - Vertical movement\n");
  printf ("\n");
  printf ("  Joystick Buttons (2-button joysticks):\n");
  printf ("    Button 0         - B button\n");
  printf ("    Button 1         - A button\n");
  printf ("\n");
  printf ("  Joystick Buttons (other joysticks):\n");
  printf ("    Button 0         - B button\n");
  printf ("    Button 1         - Select button\n");
  printf ("    Button 2         - A button\n");
  printf ("    Button 3         - Start button\n");
  printf ("    Button 4         - A button\n");
  printf ("    Button 5         - B button\n");
  printf ("    Button 6         - Select button\n");
  printf ("    Button 7         - Start button\n");
  printf ("    Button 8         - Pause or resume emulation\n");
  printf ("\n");
#ifdef HAVE_X
  printf ("  Extra keys for old X11 renderer (--renderer=old)\n");
  printf ("    F1          - Disable background\n");
  printf ("    F2          - Re-enable background\n");
  printf ("    F3          - Disable sprites\n");
  printf ("    F4          - Re-enable sprites\n");
  printf ("    F5          - Alternate tiles\n");
  printf ("    F6          - Default tiles\n");
  printf ("    F8          - Flush tile cache\n");
  printf ("\n");
#endif /* HAVE_X */
  printf ("  Extra Keys for VS UniSystem games:\n");
  printf ("    [/{         - Insert Coin (Left)\n");
  printf ("    ]/}         - Insert Coin (Right)\n");
  printf ("    |/\\         - Service Credit\n");
  printf ("    Q/q         - Toggle Dipswitch #1\n");
  printf ("    W/w         - Toggle Dipswitch #2\n");
  printf ("    E/e         - Toggle Dipswitch #3\n");
  printf ("    R/r         - Toggle Dipswitch #4\n");
  printf ("    T/t         - Toggle Dipswitch #5\n");
  printf ("    Y/y         - Toggle Dipswitch #6\n");
  printf ("    U/u         - Toggle Dipswitch #7\n");
  printf ("    I/i         - Toggle Dipswitch #8\n");
}

/****************************************************************************/

static void
restoresavedgame(void)
{
  int fd, result, x;
  char buffer[1024];
  struct stat statbuf;
  *savefile = 0;
  strncpy (buffer, homedir, 1010);
  if (*buffer != 0 && buffer[strlen (buffer) - 1] == '/')
    buffer[strlen (buffer) - 1] = 0;
  strcat (buffer, "/.tuxnes");
  result = stat (buffer, &statbuf);
  if (result == 0)
    {
      for (x = strlen (filename) - 1; x > 0 && filename[x - 1] != '/';
           x--);
      strcat (buffer, "/");
      strncat (buffer, filename + x, 1019 - strlen (buffer));
      strcat (buffer, ".sav");
      strcpy (savefile, buffer);
    }
  if (!*savefile)
    {
      strncpy (buffer, filename, 1013);
      savefile[1013] = 0;
      for (x = strlen (buffer); x >= 0 && buffer[x] != '/'; x--)
        buffer[x] = 0;
      strcat (buffer, "tuxnes.tmp");
      result = open (buffer, O_CREAT | O_RDWR, 0666);
      unlink (buffer);
      if (result < 0)
        {
          buffer[strlen (buffer) - 10] = 0;
          fprintf (stderr, "Warning: can not write to directory %s\n", buffer);
          fprintf (stderr,
                   "Create directory ~/.tuxnes if you want to save games there instead.\n");
        }
      else
        close (result);
      strncpy (savefile, filename, 1019);
      savefile[1019] = 0;
      strcat (savefile, ".sav");
    }
  if ((fd = open (savefile, O_RDWR)) >= 0)
    {
      read (fd, RAM + 0x6000, 8192);
      close (fd);
    }
  else
    {
      /* In this special case, there is a ~/.tuxnes directory but no save
         file in it.  If there is a save file in the romfile directory,
         copy it into the ~/.tuxnes directory. */
      strncpy (buffer, filename, 1019);
      buffer[1019] = 0;
      strcat (buffer, ".sav");
      if ((fd = open (buffer, O_RDWR)) >= 0)
        {
          read (fd, RAM + 0x6000, 8192);
          close (fd);
        }
      else
        {
          /* Finally, if no save file is found either place, look for a
             iNES-type save file. */
          strncpy (buffer, filename, 1023);
          buffer[1023] = 0;
          buffer[strlen (buffer) - 4] = 0;
          strcat (buffer, ".sav");
          fd = open (buffer, O_RDWR);
          read (fd, RAM + 0x6000, 8192);
          close (fd);
        }
    }
}

/****************************************************************************/

void
loadpal(char *palfile)
{
  /* for the raw palette data */
  unsigned char palette[192];
  int pen, pens;
  int len;
  int fd = -1;

  if (unisystem && !palremap)
    NES_palette = palettes[sizeof (palettes) / sizeof (*palettes) - 1].data;
  else
    NES_palette = palettes[0].data;
  if (palfile)
    {
      char *buffer;

      len = strlen(palfile) + 1;
      if (! (buffer = malloc(len)))
	{
	  perror ("loadpal: malloc");
	  return;
	}
      memcpy (buffer, palfile, len);
      palfile = buffer;
    }
  if (palfile)
    {
      if ((fd = open (palfile, O_RDONLY)) < 0)
        {
          perror (palfile);
          free (palfile);
          palfile = 0;
        }
    }
  if (paldata && !palfile)
    {
      char *buffer;

      len = strlen(filename) + 1;
      if (! (buffer = malloc(len)))
	{
	  perror ("loadpal: malloc");
	  return;
	}
      memcpy (buffer, filename, len);
      palfile = buffer;
    }
  if (!palfile)
    {
      if (palremap)
	return;
      if (!(palfile = malloc ((len = strlen (filename)) + 11)))
        {
          perror ("loadpal: malloc");
          return;
        }
      strcpy (palfile, filename);
      /* .NES -> .pal */
      if ((len > 4)
          && (palfile[len - 4] == '.')
          && (tolower(palfile[len - 3]) == tolower('N'))
          && (tolower(palfile[len - 2]) == tolower('E'))
          && (tolower(palfile[len - 1]) == tolower('S')))
        palfile[len -= 4] = '\0';
      strcat (palfile + len, ".pal");
      if ((fd = open (palfile, O_RDONLY)) < 0) {
        if ((fd = open ("tuxnes.pal", O_RDONLY)) < 0)
          {
            while (len && (palfile[len] != '/'))
              len--;
            palfile[len] = '/';
            palfile[len + 1] = '\0';
            strcat (palfile + len, "tuxnes.pal");
            if ((fd = open (palfile, O_RDONLY)) < 0)
              {
                free (palfile);
                return;
              }
          }
        else
          {
            strcpy (palfile, "tuxnes.pal");
          }
      }
    }
  if (verbose)
    {
      fprintf (stderr, "Reading palette from %s\n", palfile);
    }
  /* read the raw palette data -- it's ok if it's short */
  if ((fd < 0) && paldata)
    {
      memcpy (palette, paldata, 192);
      pens = 64;
    }
  else {
    char buf[9];
    ssize_t count;

    if ((count = read (fd, buf, 9)) < 0)
      {
        perror (palfile);
	free (palfile);
	return;
      }
    /* handler for iNES-style hexadecimal palette files */
    if ((count == 9)
	&& isxdigit(buf[0])
	&& isxdigit(buf[1])
	&& (buf[2] == ',')
	&& isxdigit(buf[3])
	&& isxdigit(buf[4])
	&& (buf[5] == ',')
	&& isxdigit(buf[6])
	&& isxdigit(buf[7])
	&& ((buf[8] == '\r')
	     || (buf[8] == '\n')
	     || isspace(buf[8])))
      {
	for (pens = 0; pens < 64; pens ++)
	  {
	    int r, g, b;

	    if (pens)
	      {
		if ((count = read (fd, buf, 9)) < 0)
		  {
		    perror (palfile);
		    free (palfile);
		    return;
		  }
		if (count == 8)
		  {
		    buf[count ++] = '\n';
		  }
                if (count < 9) break;
		while ((*buf == '\r')
		       || (*buf == '\n')
		       || isspace (*buf))
		  {
		    memmove (buf, buf + 1, 8);
		    if ((count = read (fd, buf + 8, 1)) < 0)
		      {
		        perror (palfile);
			free (palfile);
			return;
		      }
		  }
		if (count < 1) break;
		if (! (isxdigit(buf[0])
		       && isxdigit(buf[1])
		       && (buf[2] == ',')
		       && isxdigit(buf[3])
		       && isxdigit(buf[4])
		       && (buf[5] == ',')
		       && isxdigit(buf[6])
		       && isxdigit(buf[7])
		       && ((buf[8] == '\r')
			   || (buf[8] == '\n')
			   || isspace(buf[8])))) break;
	      }
	    sscanf (buf, "%x,%x,%x", &r, &g, &b);
	    palette [pens * 3] = r;
	    palette [pens * 3 + 1] = g;
	    palette [pens * 3 + 2] = b;
	  }
      }
    /* handler for Nesticle-style raw palette files */
    else
      {
        if (count) memcpy ((char *) palette, buf, count);
        if ((pens = read (fd, (char *) palette + count, 192 - count) / 3) < 0)
	  {
	    perror (palfile);
	    free (palfile);
	    return;
	  }
	pens += count / 3;
      }
    close (fd);
  }

  /* convert the palette */
  if (!(NES_palette = (unsigned int *) malloc (64 * sizeof (*NES_palette))))
    {
      perror ("malloc");
      free (palfile);
      return;
    }
  for (pen = 0; pen < 64; pen++)
    {
      if (pen < pens)
        {
          NES_palette[pen] =
            (((unsigned long) palette[pen * 3]) << 16) |
            (((unsigned long) palette[pen * 3 + 1]) << 8) |
            (((unsigned long) palette[pen * 3 + 2]));
        }
      else
        {
          if (unisystem)
            NES_palette[pen] = palettes[sizeof (palettes) / sizeof
                                        (*palettes) - 1].data[pen];
          else
            NES_palette[pen] = palettes[0].data[pen];
        }
      /* dump the loaded palette to stdout in C format, for adding to palettes[] */
      /*       printf ("0x%6.6x%s", NES_palette[pen], */
      /*               (pen < 63) ? ((pen + 1) % 4) ? ", " : ",\n" : "\n"); */
    }

  /* clean up */
  free (palfile);
}

/****************************************************************************/

static void
js_set_nesmaps(char *mapspec)
{
  enum buttonaxis_state { none = 0, button, axis };

  int js_dev = -1;
  int nes_button_i;
  int nes_button_inc = 1;
  int expect_buttonaxis = none;
  int buttonaxis = -1;
  unsigned char *s = (unsigned char *) mapspec;

  while( *s != '\0' )
    if( isdigit( *s ) && (*(s + 1) == ':' ) )
      {
	js_dev = (*s - '1');
	++s;
	if( (js_dev < 0) || (js_dev > 2) )
	  {
	    fprintf( stderr, 
		     "ignoring joystick map (limited to joystick 1 and 2)\n" );
	    continue;
	  }
	nes_button_i = 0;
	while( (*(++s) != '\0') || (expect_buttonaxis) )
	  {
	    if( expect_buttonaxis != none )
	      {
		if( isdigit( *s ) )
		  {
		    buttonaxis *= 10;
		    buttonaxis += (*s - '0');
		    continue;
		  }
		else
		  {
		    if( expect_buttonaxis == button )
		      {
			if( buttonaxis < 2 )
			  js_usermapped2button[js_dev][buttonaxis] = 1;
			expect_buttonaxis = none;
			if( buttonaxis < JS_MAX_BUTTONS ) 
			  js_nesmaps[js_dev].button[buttonaxis] = js_mapsequence[nes_button_i];
		      }
		    else		/* (expect_buttonaxis == axis) */
		      {
			expect_buttonaxis = none;
			nes_button_inc = 2;
			if( buttonaxis < JS_MAX_AXES )
			  if( (nes_button_i + 1) < JS_MAX_NES_BUTTONS )
			    {
			      js_nesmaps[js_dev].axis[buttonaxis].neg
				= js_mapsequence[nes_button_i];
			      js_nesmaps[js_dev].axis[buttonaxis].pos
				= js_mapsequence[nes_button_i + 1];
			    }
		      }
		    if( *s == '\0' )
		      break;
		  }
	      }
	    if( *s == ',' )
	      if( (nes_button_i += nes_button_inc) < JS_MAX_NES_BUTTONS )
		nes_button_inc = 1;
	      else
		break;
	    else if( *s == '+' )
	      break;
	    else if ( expect_buttonaxis == none )
	      {
		if( (*s == 'b') || (*s == 'B') )
		  expect_buttonaxis = button;
		else if( (*s == 'a') || (*s == 'A') )
		  expect_buttonaxis = axis;
		else
		  continue;
		if( isdigit( *(s+1) ) )
		  buttonaxis = 0;
		else
		  expect_buttonaxis = none;
	      }
	  }
      }
    else
      ++s;
}

/****************************************************************************/

int
main (int argc, char **argv)
{
  int x;
  int r;
  int i, basestart, baseend; /* these are for working out the base filename */
  int audiofd;
  int size;
  int cmirror;
  DIR *dir;		/* for checking if .tuxnes directory is present */
  char *palfile = 0;	/* palette file */

  char *ggcode;
  int ggret, parseret;

  /* for the Game Genie */
  int address, data, compare;

#ifdef HAVE_LIBZ
  gzFile rom_file_gz = NULL;
  unzFile rom_file_zip = NULL;
  char *extension = NULL;
#else
  int romfd;
#endif

  ggcode = NULL;

  /* set up the mapper arrays */
  InitMapperSubsystem();

  /* Find the user's home directory */
  if (!(homedir = getenv ("HOME")))
    {
      struct passwd *pwent = getpwuid (getuid ());

      if (pwent)
        homedir = pwent->pw_dir;
      if (!homedir)
        homedir = "";
    }

  /* Make sure there's a ~/.tuxnes directory */
  if ((tuxnesdir = malloc (strlen (homedir) + strlen ("/.tuxnes/") + 1))
      == NULL)
    {
      fprintf (stderr, "Out of memory\n");
      return (1);
    }
  sprintf (tuxnesdir, "%s%s", homedir, "/.tuxnes/");
  dir = opendir (tuxnesdir);
  if ((dir == NULL)
      && (errno == ENOENT))
    {
      mkdir (tuxnesdir, 0777);
      if ((dir = opendir (tuxnesdir)) == NULL)
        {
          fprintf (stderr, "Cannot open directory %s\n", tuxnesdir);
          return (1);
        }
    }
  else if ((dir == NULL)
           && (errno == ENOENT))
    {
      fprintf (stderr, "Cannot open directory %s\n", tuxnesdir);
      return (1);
    }
  else
    {
      closedir (dir);
    }

  /* initialize variables */
  verbose = 0;
  cmirror = 0;
  dolink = 0;
  disassemble = 0;
  gamegenie = 0;

  /* check for the default output device */
  if ((audiofd = open (DSP, O_WRONLY | O_APPEND)) < 0)
    sound_config.audiofile = NULL;
  else {
    sound_config.audiofile = DSP;
    close(audiofd);
  }

  /*
   * Parse args
   * the following parsing code is adapted from the getopt(3) man page
   * the long option handling has been removed, since having duplicate code
   * for equivalent long and short options can only lead to inconsistency
   */
  while (1)
    {
      int option_index = 0;
      static struct option long_options[] =
      {
        {"help", 2, 0, 'h'},
        {"controls", 0, 0, 'c'},
        {"joystick", 1, 0, 'j'},
        {"js1", 2, 0, '1'},
        {"js2", 2, 0, '2'},
	{"joystick-map", 1, 0, 'J'},
        {"verbose", 0, 0, 'v'},
        {"old", 0, 0, 'o'},
	{"ignore-unhandled", 0, 0, 'i'},
        {"link", 0, 0, 'l'},
        {"disassemble", 0, 0, 'd'},
        {"gamegenie", 1, 0, 'g'},
        {"mirror", 1, 0, 'm'},
        {"show-header", 0, 0, 'H'},
        {"mapper", 1, 0, 'M'},
        {"fix-mapper", 0, 0, 'f'},
        {"sound", 2, 0, 's'},
	{"static-color", 0, 0, 'S'},
	{"static-colour", 0, 0, 'S'},
	{"format", 1, 0, 'F'},
        {"soundrate", 1, 0, 'R'},
        {"delay", 1, 0, 'D'},
        {"palfile", 1, 0, 'p'},
        {"palette", 1, 0, 'P'},
        {"version", 0, 0, 'V'},
	{"enlarge", 2, 0, 'E'},
	{"geometry", 1, 0, 'G'},
	{"display", 1, 0, 0},
	{"scanlines", 2, 0, 'L'},
	{"renderer", 1, 0, 'r'},
	{"echo", 0, 0, 'e'},
	{"swap-inputs", 0, 0, 'X'},
	{"sticky-keys", 0, 0, 'K'},
	{"in-root", 0, 0, 'I'},
	{"bw", 0, 0, 'b'},
#ifdef HAVE_LIBM
	{"ntsc-palette", 2, 0, 'N'},
#endif
        {0, 0, 0, 0}
      };

      parseret = getopt_long (argc, argv,
#ifdef HAVE_LIBM
			      "N::"
#endif
			      "bcdfhHg:j:J:lom:M:F:R:p:P:vV1::2::s::SL::r:E::D:ieUG:XKI",
			      long_options,
			      &option_index);
      if (parseret == -1)
        break;

      switch (parseret)
        {
	case 'b':
	  monochrome = 1;
	  break;
        case 'c':
          help (*argv, "controls");
          exit (0);
          break;
	case 'X':
	  swap_inputs = 1;
	  break;
	case 'K':
	  renderer_config.sticky_keys = 1;
	  break;
	case 'I':
	  renderer_config.inroot = 1;
	  break;
        case 'd':
          disassemble = 1;
          break;
        case 'f':
          dirtyheader = 1;
          break;
        case 'h':
          help (*argv, optarg);
          exit (0);
          break;
        case 'H':
          showheader = 1;
          break;
        case 'g':
          gamegenie = 1;
          ggcode = optarg;
          break;
        case 'j':
        case '1':
          jsdevice[0] = optarg ? optarg : JS1;
          break;
        case '2':
          jsdevice[1] = optarg ? optarg : JS2;
          break;
	case 'J':
	  js_set_nesmaps( optarg );
	  break;
        case 'l':
          dolink = 1;
          break;
        case 'o':
	  rendname = "old";
          break;
	case 'i':
	  ignorebadinstr = 1;
	  break;
        case 'm':
          switch (*optarg ? optarg[1] ? 0 : *optarg : 0)
            {
            case 'h':
              cmirror = 1;
              break;
            case 'v':
              cmirror = 2;
              break;
            case 's':
              cmirror = 3;
              break;
            case 'n':
              cmirror = 4;
              break;
            default:
              fprintf (stderr, "bad mirror argument: %s\n", optarg);
              exit (2);
              break;
            }
          break;
        case 'M':
          mapperoverride = 1;
          MAPPERNUMBER = atoi (optarg);
          break;
        case 'v':
          verbose = 1;
          break;
        case 's':
          if (optarg &&
              ((strcmp (optarg, "mute") == 0)
            || (strcmp (optarg, "none") == 0)))
            sound_config.audiofile = NULL;
          else
            sound_config.audiofile = optarg ? optarg : DSP;
          break;
	case 'S':
	  indexedcolor = 0;
	  break;
	case 'F':
	  sample_format_name = optarg;
	  break;
	case 'R':
	  sound_config.audiorate = atoi (optarg);
	  if (sound_config.audiorate < 1)
	    {
	      fprintf (stderr, "%s: not a valid audio rate (need a positive integer)\n", optarg);
	      exit (2);
	    }
	  break;
	case 'D':
	  sound_config.max_sound_delay = atof (optarg);
	  if (sound_config.max_sound_delay < 0.0)
	    {
	      fprintf (stderr, "%s: not a valid delay (must be non-negative))\n", optarg);
	      exit (2);
	    }
	  break;
	case 'e':
	  sound_config.reverb = 1;
	  break;
        case 'p':
          palfile = optarg ? optarg : 0;
          NES_palette = 0;
          break;
        case 'P':
          if (optarg && *optarg)
            {
              int i;
              unsigned int *partial = 0;
              int partials = 0;
              int len;

              palfile = 0;
              NES_palette = 0;
              len = strlen (optarg);
              for (i = 0; i < (sizeof (palettes) / sizeof (*palettes)); i++)
                if (!strcmp (palettes[i].name, optarg))
                  {
                    NES_palette = palettes[i].data;
                    break;
                  }
                else if (!strncmp (palettes[i].name, optarg, len))
                  {
                    partial = palettes[i].data;
                    partials++;
                  }
              if ((partials == 1) && !NES_palette)
                NES_palette = partial;
              if (!NES_palette)
                {
                  if (partials)
                    fprintf (stderr, "%s: palette name `%s' is ambiguous\n",
                             *argv, optarg);
                  else
                    fprintf (stderr, "%s: unrecognized palette name `%s'\n",
                             *argv, optarg);
                  exit (2);
                }
            }
          else
            NES_palette = palettes[0].data;
          break;
        case 'V':
	  help_version (0);
          exit (0);
          break;
	case 'G':
	  renderer_config.geometry = optarg;
	  break;
	case 'E':
	  if (optarg && *optarg)
	    magstep = atoi (optarg);
	  else
	    magstep = 2;
	  break;
	case 'L':
	  if (optarg && *optarg)
	    scanlines = atoi (optarg);
	  else
	    scanlines = 0;
	  break;
	case 'r':
	  rendname = optarg;
	  break;
#ifdef HAVE_LIBM
	case 'N':
          if (optarg)
            {
              char *p;
	      double foo;

	      foo = strtod(optarg, &p);
	      if (optarg != p)
	        {
		  hue = foo;
		  while (hue < 0.0) hue += 360.0;
		  while (hue >= 360.0) hue -= 360.0;
		}
	      if (*p == ',')
	        {
		  optarg = p+1;
		  foo = strtod(optarg, &p);
		  if (optarg != p)
		    {
                      tint = foo;
		    }
                  if (*p || (tint < 0.0) || (tint > 1.0))
                    {
	              fprintf (stderr, "%s: not a valid tint level (must be a number from 0 to 1)\n", optarg);
		      exit (2);
		    }
		}
	      else if (*p)
	        {
	          fprintf (stderr, "%s: not a valid hue angle (must be an angle in degrees)\n", optarg);
		  exit (2);
		}
	    }
	  NES_palette = ntsc_palette(hue, tint);
	  break;
#endif
	case 0: /* long options with no short equivalents */
	  if (! strcmp(long_options[option_index] . name, "display"))
	    {
	      renderer_config.display_id = optarg;
	      break;
	    }
        default:
          fprintf (stderr, USAGE, *argv);
          exit (2);
          break;
        }
    }

  if ((scanlines != 100) && ! magstep)
    {
      magstep = 2;
    }

  if ((argc - optind) != 1)
    {
      fprintf (stderr, "%s: too %s arguments\n", *argv, (argc > optind) ? "many" : "few");
      fprintf (stderr, USAGE, *argv);
      exit (2);
    }
  filename = argv[optind];

  /*
    This initializes the base filename by taking the filename of the ROM and
    parsing of leading directory information as well as the trailing extension
    (.nes, .gz, .Z).
  */
  basestart = baseend = 0;
  for (i = 0; i < strlen(filename); i++)
    {
      if (filename[i] == '/')
        {
          basestart = i + 1;
        }
      else if (filename[i] == '.')
        {
          baseend = i - 1;
        }
    }

    if (! (basefilename = malloc(baseend - basestart + 1)))
      {
        perror ("main: malloc");
        exit (1);
      }

    for (i = 0; i <= (baseend - basestart); i++)
      {
        basefilename[i] = filename[basestart + i];
      }
    basefilename[baseend - basestart + 1] = '\0';

  /* initialize joysticks */
  {
    int stick;

    for (stick = 0; stick < 2; stick ++)
      {
	if (jsdevice[stick])
	  {
#ifdef HAVE_LINUX_JOYSTICK_H
	    if ((jsfd[stick] = open (jsdevice[stick], O_RDONLY)) < 0)
	      {
		perror (jsdevice[stick]);
	      }
	    else
	      {
		ioctl (jsfd[stick], JSIOCGVERSION, jsversion + stick);
		ioctl (jsfd[stick], JSIOCGAXES, jsaxes + stick);
		ioctl (jsfd[stick], JSIOCGBUTTONS, jsbuttons + stick);
		fcntl (jsfd[stick], F_SETFL, O_NONBLOCK);    /* set joystick to non-blocking */
		if( jsbuttons[stick] == 2 )
		  {		/* modify button map for a 2-button joystick */
		    if( !js_usermapped2button[stick][0] )
		      js_nesmaps[stick].button[0] = BUTTONB;
		    if( !js_usermapped2button[stick][1] )
		      js_nesmaps[stick].button[1] = BUTTONA;
		  }

		if (verbose)
		  {
		    fprintf (stderr,
			     "Joystick %d (%s) has %d axes and %d buttons. Driver version is %d.%d.%d.\n",
			     stick + 1,
			     jsdevice[stick],
			     jsaxes[stick],
			     jsbuttons[stick],
			     jsversion[stick] >> 16,
			     (jsversion[stick] >> 8) & 0xff,
			     jsversion[stick] & 0xff);
		  }
	      }
#else
	    fprintf (stderr,
		     "Joystick support was disabled at compile-time. To enable it, make sure\n"
		     "you are running a recent version of the Linux kernel with the joystick\n"
		     "driver enabled, and install the header file <linux/joystick.h>.\n");
#endif /* HAVE_LINUX_JOYSTICK_H */
	  }
      }
  }

  /* Select a sound sample format */
  {
    struct SampleFormat *match = 0;
    int partials = 0;
    int len;
    
    len = strlen (sample_format_name);
    for (sample_format = sample_formats; sample_format->name; sample_format++)
      if (!strcmp (sample_format->name, sample_format_name))
        {
	  break;
	}
      else if (!strncmp (sample_format->name, sample_format_name, len))
        {
	  match = sample_format;
	  partials++;
	}
    if (*sample_format_name && !sample_format->name)
      {
        char *tail;
	int sample_format_number;

	sample_format_number = strtoul (sample_format_name, &tail, 0);
	if (! *tail)
	  {
	    for (sample_format = sample_formats; sample_format->name; sample_format ++)
	      {
	        if (sample_format_number == sample_format->number) break;
	      }
	  }
      }
    if ((partials == 1) && !sample_format->name)
      sample_format = match;
    if (!sample_format->name)
      {
	if (partials)
	  fprintf (stderr, "%s: sound sample format name `%s' is ambiguous\n",
		   *argv, sample_format_name);
	else
	  fprintf (stderr, "%s: unrecognized sound sample format name `%s'\n",
		   *argv, sample_format_name);
	exit (2);
      }
  }

  /* Open ROM file */
#ifdef HAVE_LIBZ
	extension = strrchr(filename,'.');

	/* If the extension is .zip open the file */
	if (extension && !strcasecmp(extension,".zip")) {
		rom_file_zip = unzOpen(filename);
		if (rom_file_zip == NULL) {
			perror(filename);
			exit(EXIT_FAILURE);
		}
	}
	/* If its not a .zip file treat it as a .gz */
	else {
		rom_file_gz = gzopen (filename, "rb");
		
		if (rom_file_gz == NULL)
		{
			perror (filename);
			exit (EXIT_FAILURE);
		}
	}
	
  /* FIXME: Error */
#else
  romfd = open (filename, O_RDONLY);

  if (romfd < 0)
    {
      perror (filename);
      exit (1);
    }
#endif

#if HAVE_LIBZ
  /*
   * FIXME: Do not calculate the size, instead read the complete file with
   * getc(), this looks wrong but working.
   */
#else
  size = lseek (romfd, 0, SEEK_END);
  lseek (romfd, 0, SEEK_SET);   /* Get file size */

  if (size < 0)
    {
      perror (filename);
      exit (1);
    }
  if (size == 0)
    {
      fprintf (stderr, "Unable to read %s (empty file)\n", filename);
      exit (1);
    }
#endif

  /* allocate space for the ROM */
  if ((r = (int) mmap (ROM, 0x300000,
		       PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_FIXED | MAP_PRIVATE | MAP_ANON,
		       -1, 0)) < 0)
    {
      perror ("mmap");
      exit (1);
    }

  /* load the ROM */
#if HAVE_LIBZ
	
	
	/* Treat as a .zip file */
	if (extension && !strcasecmp(extension,".zip")) {
		ziploader(rom_file_zip,filename);
		unzClose(rom_file_zip);
	}
	/* Treat as a .gz file */
	else {
		int c;
		int i = 0;
		while ((c = gzgetc (rom_file_gz)) != -1)
		{
			ROM[i++] = c;
		}
		size = i;

		gzclose (rom_file_gz);
	}

#else
  if (read (romfd, ROM, size) != size)
    {
      perror (filename);
      exit (1);
    }

  close (romfd);
#endif

  if (verbose)
    fprintf (stderr, "Rom size: %d\n", size);

  /* Allocate memory */
  r = (int) mmap (RAM, 0x8000,
		  PROT_READ | PROT_WRITE,
		  MAP_FIXED | MAP_PRIVATE | MAP_ANON,
		  -1, 0);
  if (r <= 0)
    {
      perror ("mmap");
      exit (1);
    }
  r = (int) mmap (CODE_BASE, 0x800000,
		  PROT_READ | PROT_WRITE | PROT_EXEC,
                  MAP_FIXED | MAP_PRIVATE | MAP_ANON,
		  -1, 0);
  if (r <= 0)
    {
      perror ("mmap");
      exit (1);
    }
  r = (int) mmap ((void *) INT_MAP, 0x400000,
		  PROT_READ | PROT_WRITE,
                  MAP_FIXED | MAP_PRIVATE | MAP_ANON,
		  -1, 0);
  if (r <= 0)
    {
      perror ("mmap");
      exit (1);
    }

  /* Initialize sound playback */
  if (InitAudio (argc, argv))
  {
       fprintf (stderr,
		"%s: warning: failed to initialize sound playback\n",
		*argv);
  }

  /* if the user requested header bytes, show them */
  if (showheader)
    {
      fprintf (stderr,
               "iNES header bytes:\n  0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F\n  ");
      for (x = 0; x < 16; x++)
        {
          fprintf (stderr, "%02X  ", ROM[x]);
        }
      fprintf (stderr, "\n ");
      for (x = 0; x < 16; x++)
        {
          if ((ROM[x] >= 0x20) && (ROM[x] <= 0x7F))
            fprintf (stderr, " %c  ", ROM[x]);
          else
            fprintf (stderr, "    ");
        }
      fprintf (stderr, "\n");
    }

  /* Initialize ROM mapper */
  MAPTABLE[0] = RAM;
  MAPTABLE[1] = RAM;
  MAPTABLE[2] = RAM;
  MAPTABLE[4] = RAM;
  MAPTABLE[16] = RAM - 65536;   /* Wrap around accesses which index above FFFF */
  MAPTABLE[6] = MAPTABLE[7] = RAM;      /* Save RAM */
  MAPTABLE[3] = RAM + 0x2000;
  MAPTABLE[5] = RAM;            /* 3xxx and 5xxx really shouldn't be mapped to anything, but they're mapped to some unused ram just to prevent pagefaults by weird code that tries to access these areas. */
  SRAM_ENABLED = 1;
  if (!strncmp ("NES\x1a", (char *) ROM, 4))    /* .NES */
    {
      int used;

      ROM_BASE = ROM + 16;      /* 16 bytes for .nes header */

      /* check for battery-backed SRAM */
      if (!(ROM[6] & 2))
        SRAM_ENABLED = 0;

      /* check for a 512-byte trainer */
      if (ROM[6] & 4)
        {
          if (verbose)
            {
              fprintf (stderr, "512-byte trainer present\n");
            }
          memcpy (RAM + 0x7000, ROM_BASE, 512);
          ROM_BASE += 512;
        }

      /* figure out the mapper */
      if (!mapperoverride && dirtyheader)
        MAPPERNUMBER = ROM[6] >> 4;
      else if (!mapperoverride)
        {
          MAPPERNUMBER = (ROM[6] >> 4) | (ROM[7] & 0xf0);
          if ((ROM[7] & 0x0c) || ROM[8] | ROM[9])
            fprintf (stderr, "Warning: may need -f to force 4-bit mapper\n");
          /* there's actually a defined header bit past ROM[7]! */
          if ((ROM[10] & 0x10) && verbose)
            fprintf (stderr,
                     "Cartridge has bus conflicts (not emulated)\n");
          switch (ROM[7] & 0x03)
            {
            case 0x00:
              if (verbose)
                fprintf (stderr, "NES/Famicom ROM\n");
              break;
            case 0x01:
              if (verbose)
                fprintf (stderr, "VS UniSystem ROM\n");
              unisystem = 1;
              break;
            case 0x02:
              if (verbose)
                fprintf (stderr, "PlayChoice-10 ROM\n");
              break;
            default:
              fprintf (stderr, "Warning: unrecognized system ID %d\n",
                       ROM[7] & 0x03);
            }
        }
      if (MAPPERNUMBER == 99)
        {
          unisystem = 1;
        }

      VROM_BASE = ROM_BASE + ROM[4] * 16384;
      if (ROM[5])
        memcpy (VRAM, VROM_BASE, 8192);
      ROM_PAGES = ROM[4];
      VROM_PAGES = ROM[5];

      /* calculate used space in image */
      used = 16                    /* header size */
	+ ((ROM[6] & 4) ? 512 : 0) /* trainer size */
	+ 16384 * ROM_PAGES        /* PRG-ROM pages */
	+ 8192 * VROM_PAGES;       /* CHR-ROM pages */

      /* handle palette remapping/redefinition tables */
      if (size == (used + 64)) {
	palremap = ROM + used;
      } else if (size == (used + 192)) {
	paldata = ROM + used;
      } else if ((size != used) && verbose) {
	fprintf (stderr, "File contains %d extra bytes\n",
		 size - used);
      }

      hvmirror = (ROM[6] & 1) ^ 1;
      nomirror = ROM[6] & 8;
    }
  else if (!strncmp ("FDS\x1a", (char *) ROM, 4))       /* .FDS */
    {
      fprintf (stderr,
            "Famicom Disk System (FDS) disk images aren't yet supported\n");
      exit (1);
    }
  else if (size == 40960)
    {
      /* No header, assume raw image */
      ROM_BASE = ROM;
      MAPPERNUMBER = 0;
      memcpy (VRAM, ROM + 32768, 8192);
      ROM_PAGES = 2;
      VROM_PAGES = 2;
      VROM_BASE = ROM + 32768;
      nomirror = 1;
      mapmirror = 0;
    }
  else
    {
      fprintf (stderr, "Unrecognized ROM file format\n");
      exit (1);
    }

  if (MAPPERNUMBER > MAXMAPPER || ((int *) MapperInit)[MAPPERNUMBER] == 0)
    {
      fprintf (stderr, "Unknown Mapper: %d (0x%02X)\n", MAPPERNUMBER,
               MAPPERNUMBER);
      exit (1);
    }
  if (verbose)
    {
      fprintf (stderr, "Mapper %d (%s)  Program ROM %dk  Video ROM %dk\n",
               MAPPERNUMBER, MapperName[MAPPERNUMBER], ROM_PAGES * 16,
               VROM_PAGES * 8);
    }

  /* How many ROM address bits are used */
  if (ROM_PAGES <= 2)
    ROM_MASK = 1;
  else if (ROM_PAGES <= 4)
    ROM_MASK = 3;
  else if (ROM_PAGES <= 8)
    ROM_MASK = 7;
  else if (ROM_PAGES <= 16)
    ROM_MASK = 15;
  else if (ROM_PAGES <= 32)
    ROM_MASK = 31;
  else
    ROM_MASK = 63;

  /* How many VROM address bits are used */
  if (VROM_PAGES <= 2)
    VROM_MASK = 1;
  else if (VROM_PAGES <= 4)
    VROM_MASK = 3;
  else if (VROM_PAGES <= 8)
    VROM_MASK = 7;
  else if (VROM_PAGES <= 16)
    VROM_MASK = 15;
  else if (VROM_PAGES <= 32)
    VROM_MASK = 31;
  else if (VROM_PAGES <= 64)
    VROM_MASK = 63;
  else
    VROM_MASK = 127;

  /* VROM mask for 1k-sized pages */
  VROM_MASK_1k = (VROM_MASK << 3) | 7;

  ((void (*)(void))(MapperInit[MAPPERNUMBER]))();

  if (cmirror == 1 && mapmirror == 0)
    {
      if (verbose && !hvmirror)
        fprintf (stderr, "Overriding default vertical mirroring.\n");
      hvmirror = 1;
    }
  if (cmirror == 2 && mapmirror == 0)
    {
      if (verbose && hvmirror)
        fprintf (stderr, "Overriding default horizontal mirroring.\n");
      hvmirror = 0;
    }
  if (cmirror == 3 && mapmirror == 0)
    {
      if (verbose && !nomirror && !osmirror)
        fprintf (stderr, "Overriding default mirroring.\n");
      osmirror = 1;
    }
  if (cmirror == 4 && mapmirror == 0)
    {
      if (verbose && !nomirror)
        fprintf (stderr, "Overriding default mirroring.\n");
      nomirror = 1;
    }
  if (verbose && mapmirror == 0)
    {
      if (nomirror)
        fprintf (stderr, "Using no mirroring.\n");
      else if (osmirror)
        fprintf (stderr, "Using one-screen mirroring.\n");
      else if (hvmirror)
        fprintf (stderr, "Using horizontal mirroring\n");
      else
        fprintf (stderr, "Using vertical mirroring\n");
    }

  if (SRAM_ENABLED)
    restoresavedgame ();
  if (!NES_palette)
    loadpal (palfile);
  if (verbose)
    {
      int i;

      for (i = 0; i < (sizeof (palettes) / sizeof (*palettes)); i++)
        if (NES_palette == palettes[i].data)
          {
            fprintf (stderr, "Using built-in palette \"%s\"\n", palettes[i].name);
            break;
          }
    }
  if (palremap) {
    unsigned int *new_palette;
    int pen;
    
    if (!(new_palette = (unsigned int *) malloc (64 * sizeof (*new_palette))))
      {
	fprintf(stderr, "Can't remap palette: ");
	fflush (stderr);
	perror ("malloc");
      }
    else
      {
	if (verbose)
	  fprintf (stderr, "Remapping palette\n");
	for (pen = 0; pen < 64; pen ++)
	  new_palette[pen] = (palremap[pen] <= 64)
	    ? NES_palette[palremap[pen]]
	    : NES_palette[pen];
	memcpy ((void *)NES_palette,
		(void *)new_palette,
		64 * sizeof(*NES_palette));
	free (new_palette);
      }
  }

  /* (Possibly) convert palette to monochrome */
  if (monochrome)
    {
      int pen;

      for (pen = 0; pen < 64; pen ++)
	{
	  unsigned long red, blue, green, gray;

	  red = (NES_palette[pen] & 0xFF0000) >> 16;
	  green = (NES_palette[pen] & 0xFF00) >> 8;
	  blue = NES_palette[pen] & 0xFF;
	  gray = 0.299 * red + 0.587 * green + 0.114 * blue;
	  if (gray > 0xFFU)
	    gray = 0xFFU;
	  NES_palette[pen] = (gray << 16) | (gray << 8) | gray;
	}
    }

  /* enter the Game Genie codes */
  if (gamegenie)
    {
      ggret = DecodeGameGenieCode (ggcode, &address, &data, &compare);
      if (ggret == GAME_GENIE_BAD_CODE)
        {
          fprintf (stderr, "invalid Game Genie code: %s\n", ggcode);
        }
      else if (ggret == GAME_GENIE_8_CHAR)
        {
          if (verbose)
            {
              fprintf (stderr, "Game Genie: address = %04X, data = %02X\n",
                       address, data);
              fprintf (stderr, "Game Genie: compare value = %02X\n", compare);
              fprintf (stderr, "Game Genie: value at %04X = %02X\n",
                       address, MAPTABLE[address >> 12][address]);
            }
          if (MAPTABLE[address >> 12][address] == compare)
            {
              MAPTABLE[address >> 12][address] = data;
              if (verbose)
                {
                  fprintf (stderr, "Game Genie: replacing...\n");
                  fprintf (stderr, "Game Genie: value at %04X = %02X\n",
                           address, MAPTABLE[address >> 12][address]);
                }
            }
        }
      else
        {
          if (verbose)
            {
              fprintf (stderr, "Game Genie: address = %04X, data = %02X\n",
                       address, data);
              fprintf (stderr, "Game Genie: replacing...\n");
            }
          MAPTABLE[address >> 12][address] = data;
          if (verbose)
            fprintf (stderr, "Game Genie: value at %04X = %02X\n", address,
                     MAPTABLE[address >> 12][address]);
        }
    }

  /* Choose renderer */
  {
       int matches = 0; /* number of matches */
       struct Renderer *match = 0; /* first match */

       for (renderer = renderers; renderer->name; renderer++)
	    if (!strcmp (renderer->name, rendname)) {
		 match = renderer;
		 matches = 1;
		 break;
	    } else if (!strncmp (renderer->name, rendname, strlen(rendname))) {
		 match = renderer;
		 matches++;
	    }
       if (matches != 1) {
	    if (matches)
		 fprintf (stderr, "%s: renderer name `%s' is ambiguous\n",
			  *argv, rendname);
	    else
		 fprintf (stderr, "%s: unrecognized renderer name `%s'\n",
			  *argv, rendname);
	    fprintf (stderr, USAGE, *argv);
	    exit (2);
       }
       renderer = match;
  }

  /* Initialize graphic display */
  if (renderer->InitDisplay (argc, argv))
  {
       fprintf (stderr,
		"%s: failed to initialize renderer `%s'\n",
		*argv, renderer->name);
       exit (1);
  }

  /* trap traps */
  if (! disassemble)
    if ((oldtraphandler = signal (SIGTRAP, &traphandler)) == SIG_ERR)
      {
	perror ("signal");
      }

  /* start the show */
  STACKPTR = (int) STACK + 0xFF;
  translate (*((unsigned short *) (MAPTABLE[15] + 0xFFFC)));
  START();                     /* execute translated code */

  /* Not Reached, but return something anyway to get rid of warnings */
  return(0);
}