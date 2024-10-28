// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: A bunch of global variables
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include "consts.h"

/* Asm linkage */
extern void     *INPUT;
extern void     *OUTPUT;
extern void     *U;
extern void     *NMI;
extern unsigned int      MAPPERNUMBER;
extern unsigned char    *ROM_BASE;
extern void     (*const MapperInit[])(void);
extern void     (*const Mapper[])(void);
extern void     (*drawimage)(int);

/* Global Variables */
extern unsigned short int hscroll[], vscroll[];
extern unsigned char     linereg[];
extern unsigned char     spriteram[];
extern unsigned char     vram[];
extern const unsigned int *NES_palette;
extern unsigned char    *MAPTABLE[];
extern unsigned int      mapmirror;
extern unsigned int      hvmirror;
extern unsigned int      nomirror;
extern unsigned int      osmirror;
extern unsigned int      CLOCK;
extern unsigned int      bpp;
extern unsigned int      bpu;
extern unsigned int      bytes_per_line;
extern unsigned int      lsb_first;
extern unsigned int      lsn_first;
extern unsigned int      pix_swab;
extern unsigned int      frameskip;
extern unsigned int      vline;
extern unsigned int      vscan;
extern unsigned int      vwrap;
extern unsigned char     vscrollreg;
extern unsigned char     hscrollreg;
extern char     *rfb;
extern char     *fb;
extern int      *palette;
extern char     *tuxnesdir;
extern char *basefilename;  /* filename without extension */

/* Flags */
extern int      verbose;
extern int      disassemble;
extern int      ignorebadinstr;
extern int      unisystem;

/* Asm linkage */
extern unsigned int     STACKPTR;   /* Store 6502 stack pointer */
extern void     *XPC;               /* Translated program counter */
extern signed int       INRET;        /* Input data return value */
extern signed int       CTNI;         /* Cycles to next interrupt */

extern unsigned char *RAM;
extern unsigned char *ROM;
extern unsigned char *CODE_BASE;
extern unsigned int  *INT_MAP;

#define ZPMEM     (RAM+0x0000)      /* Zero page */
#define STACK     (RAM+0x0100)      /* stack page */
#define REG1      (RAM+0x2000)
#define REG2      (RAM+0x4000)
#define NVRAM     (RAM+0x6000)      /* Battery RAM */
#define VRAM      vram              /* Video memory */

/* configuration variables */
struct configvars {
	char *tuxnesdir;  /* the home directory */
	char *savegamedir; /* directory for saved game files */
	char *screenshotdir;  /* directory for screenshot files */
	char *audiodevice;   /* audio device */
	char *jsdevice;      /* joystick device */
};
extern struct configvars config;

#endif
