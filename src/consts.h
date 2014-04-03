/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: Miscellaneous constants for the TuxNES program.
 */

#ifndef _CONSTS_H_
#define _CONSTS_H_

#define MAXMAPPER 255

/* only 1x1 or 2x2 integer scaling is permitted for now */
#define maxsize 2

#define tilecachedepth 25

/* Constants */
#define HCYCLES	341	/* PPU cycles per scanline (113.6666.. CPU cycles) */
#define VLINES	262	/* Total scanlines (including vblank) */
#define VBL	27428	/* CPU cycles until VBlank interrupt */
#define CPF	29781	/* CPU cycles per frame (actually 29780.6666..) */
#define PBL	81840	/* PPU cycles per frame (not counting vblank) */
#define PPF	89342	/* PPU cycles per frame */

#define BUTTONA		0x01
#define BUTTONB		0x02
#define SELECTBUTTON	0x04
#define STARTBUTTON	0x08
#define UP		0x10
#define DOWN		0x20
#define LEFT		0x40
#define RIGHT		0x80

/* prettified package name */
#define PRETTY_NAME "TuxNES"

/* constant filenames */
#define JS1 "/dev/js0"
#define JS2 "/dev/js1"
#define DSP "/dev/dsp"

/* joystick dead-zone */
#define JS_IGNORE 8192

/* joystick mapping */
#define JS_MAX_BUTTONS     32
#define JS_MAX_AXES        8
#define JS_MAX_NES_BUTTONS 9
#define PAUSEDISPLAY 0xff

#endif /* _CONSTS_H_ */
