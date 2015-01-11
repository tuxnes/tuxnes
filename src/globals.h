/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: A bunch of global variables
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include "consts.h"

/* Asm linkage */
extern int	 TRANS_TBL[];
extern void	*INPUT;
extern void	*OUTPUT;
extern void	*U;
extern void	*NMI;
extern void     *Mapper[MAXMAPPER + 1];
/* extern int	 Mapper[]; */
extern int	 MAPPERNUMBER;
extern unsigned char	*ROM_BASE;
/* extern void     *MapperInit[MAXMAPPER + 1]; */
extern void	(*MapperInit[])(void);
extern void	(*drawimage)(int);

/* Global Variables */
extern unsigned short int hscroll[], vscroll[];
extern unsigned char	 linereg[];
extern unsigned char	 spriteram[];
extern unsigned char	 vram[];
extern unsigned int	*NES_palette;
extern unsigned char	*MAPTABLE[];
extern unsigned int	 hvmirror;
extern unsigned int	 nomirror;
extern unsigned int	 osmirror;
extern unsigned int	 CLOCK;
extern unsigned int	 bpp;
extern unsigned int	 bpu;
extern unsigned int	 bitmap_pad;
extern unsigned int	 depth;
extern unsigned int      bytes_per_line;
extern unsigned int	 lsb_first;
extern unsigned int	 lsn_first;
extern unsigned int	 pix_swab;
extern unsigned int	 frameskip;
extern unsigned int	 vline;
extern unsigned int	 vscan;
extern unsigned int	 vwrap;
extern unsigned int	 scanpage;
extern unsigned int      scanlines;
extern unsigned char	 vscrollreg;
extern unsigned char	 hscrollreg;
extern char	*rfb;
extern char	*fb;
extern int	*palette, *palette2;
extern char	*tuxnesdir;
extern char *basefilename;  /* filename without extension */

/* Flags */
extern int      verbose;
extern int	disassemble;
extern int	ignorebadinstr;
extern int	unisystem;

/* Asm linkage */
extern unsigned int	STACKPTR;   /* Store 6502 stack pointer */
extern void	*XPC;               /* Translated program counter */
extern signed int	INRET;        /* Input data return value */
extern signed int	CTNI;         /* Cycles to next interrupt */
extern unsigned int	VRAMPTR;    /* address to read/write video memory */

/* configuration variables */
struct configvars
{
  char *tuxnesdir;  /* the home directory */
  char *savegamedir; /* directory for saved game files */
  char *screenshotdir;  /* directory for screenshot files */
  char *audiodevice;   /* audio device */
  char *jsdevice;      /* joystick device */
};
extern struct configvars config;

#endif
