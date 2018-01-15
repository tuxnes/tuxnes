/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: Miscellaneous constants for the TuxNES program.
 */

#ifndef CONSTS_H
#define CONSTS_H

#define MAXMAPPER 255

/* Constants */
#define HCYCLES 341     /* PPU cycles per scanline (113.6666.. CPU cycles) */
#define VLINES  262     /* Total scanlines (including vblank) */
#define VBL     27428   /* CPU cycles until VBlank interrupt */
#define CPF     29781   /* CPU cycles per frame (actually 29780.6666..) */
#define PBL     81840   /* PPU cycles per frame (not counting vblank) */
#define PPF     89342   /* PPU cycles per frame */

#endif
