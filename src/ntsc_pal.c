/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: NES palette generator
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBM

#include <math.h>
#include "globals.h"

/* It mathematically generates an NES palette that can be used by */
/* NESticle and other emulators that use the palette file.  Only the */
/* first 64 colours are set, and they are stored in triplets... i.e. */
/* RGB, RGB, RGB.  Each byte is an 8 bit colour value. */
/* Luma values were measured from my NES using the chroma/luma seperator */
/* on the filter matrix from Portendo's colour decoder board. */
/* A value of 1.0 was assigned to the brightest white which is colour */
/* 20h or 30h (both are the same). */
/* Chroma was found to be constant for all colours, which is what I */
/* expected.  I fudged this at .5 which seemed to be about right. (see */
/* farther down for more details on this) */
/* The last thing I will do will be figure out what those intensity */
/* and monochrome bits *really* do. :-)  Stay tuned for that at a */
/* later date. */


/**********************************************************/
/* Read our colour angles from the data table.  First data entry goes into */
/* cols(1), second data entry goes into cols(2), etc */
static const double cols[16] = {0, 240, 210, 180, 150, 120, 90, 60, 30, 0, 330, 300, 270, 0, 0, 0};

/**********************************************************/
/* Our brightness table. */
/* br1 is for X0h colours   br1(0) is for colour 00h, br1(1) is for 10h, etc */
/* br2 is for X1h thru XCh */
/* br3 is for XDh */
static const double br[3][4] = {
	{0.50, 0.75, 1.00, 1.00},
	{0.29, 0.45, 0.73, 0.90},
	{0.00, 0.24, 0.47, 0.77}
};

static unsigned int ntsc_palette_data[64];

unsigned int *
ntsc_palette(double hue, double tint)
{

/******/
/* Our initial hue value.  Due to the way this is calculated, only use */
/* positive values here.  332 seems to be the best "standard" setting. */
/* A range of +/- 15-20 degrees, in about 200-300 steps would probably be */
/* fairly good "user adjustment" range here. */
/* This adjusts the colours. */

/* double hue = 332.0; */

/******/
/* Our initial tint value.  Only values from 0 to 1 should be entered here. */
/* 0=least amount of saturation, 1=most.  .5 seems to be the best "standard" */
/* setting here.  .25 to .75 or maybe a bit less in 200-300 steps would */
/* probably be a good range for "user adjustment". */
/* This adjusts how much white is in the colours. */

/* double tint = 0.5; */

/**********************************************************/
/* OK, this is what you were waiting for :-)  The action thingy that calcs */
/* our palette.  Ok, in the code, X is the "brightness bits" of the colour, */
/* and Z is our actual colour... i.e. XZh represents the NES colour value. */

	int x, z;

	for (x=0; x<4; x++) {
		for (z=0; z<16; z++) {
			double s, y, theta;
			unsigned int r, g, b;

			/* grab tint */
			s = tint;
			/* grab default luminance */
			y = br[1][x];
			/* is it colour XDh? if so, get luma */
			if (z == 0) { s = 0.0; y = br[0][x]; }
			/* is it colour X0h? if so, get luma */
			if (z == 13) { s = 0.0; y = br[2][x]; }
			/* is it colour XEh? if so, set to black */
			/* is it colour XFh? if so, set to black */
			if ((z == 14) || (z == 15)) { y = s = 0.0; }
/******/
/* This does a couple calculations. (working out from the parens) */
/* first, it grabs the angle for the colour we're working on out of the */
/* array that holds our angles.  Then, it divides by 180 and multiplies by */
/* pi to turn the angle which was in degrees, into radians. */

			theta = M_PI * ((cols[z] + hue) / 180.0);

/******/
/* This next part is where the real work is done.  It is a mathematical */
/* representation of an NTSC chroma decoder matrix.  It converts Hue, */
/* Saturation, and Luminance to raw RGB values. */
/******/
/* RGB must be normalized into a value from 00h thru FFh for our VGA cards :-) */
/* First, we multiply by 256, then make sure they don't wrap or go negative. */
/* Finally, they are integerized. */

			r = 256 * (y + s * sin(theta));
			g = 256 * (y - (27.0 / 53.0) * s * sin(theta) + (10.0 / 53.0) * s * cos(theta));
			b = 256 * (y - s * cos(theta));

			if (r > 255) r = 255;
			if (g > 255) g = 255;
			if (b > 255) b = 255;
			if (r < 0) r = 0;
			if (g < 0) g = 0;
			if (b < 0) b = 0;

			ntsc_palette_data[(x<<4)|z] = (r << 16) | (g << 8) | b;

/* The variables R, G, and B now contain our actual RGB values. */
		}
	}
	return ntsc_palette_data;
}

#endif /* HAVE_LIBM */
