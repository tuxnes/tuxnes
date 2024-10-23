// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: NES palette generator
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBM
#include <math.h>

/*
 * Convert component value nominally in the range [0.0, 1.0] to 8bpc.
 */
static unsigned int
component(double c)
{
	unsigned int x = 256 * c;
	if (x > 0xffU) x = 0xffU;
	return x;
}

/*
 * Mathematically generate an NES palette that can be used by
 * NESticle and other emulators that use the palette file.  Only the
 * first 64 colours are set, and they are stored in triplets... i.e.
 * RGB, RGB, RGB.  Each byte is an 8 bit colour value.
 * Luma values were measured from my NES using the chroma/luma separator
 * on the filter matrix from Portendo's colour decoder board.
 * A value of 1.0 was assigned to the brightest white which is colour
 * 20h or 30h (both are the same).
 * Chroma was found to be constant for all colours, which is what I
 * expected.  I fudged this at .5 which seemed to be about right. (see
 * farther down for more details on this)
 * The last thing I will do will be figure out what those intensity
 * and monochrome bits *really* do. :-)  Stay tuned for that at a
 * later date.
 */
void
ntsc_palette(double hue, double tint, unsigned int outbuf[64])
{
	/*
	 * Our initial hue value.  332 seems to be the best "standard" setting.
	 * A range of +/- 15-20 degrees, in about 200-300 steps would probably be
	 * fairly good "user adjustment" range here.
	 * This adjusts the colours.
	 */
	/* double hue = 332.0; */

	/*
	 * Our initial tint value.  Only values from 0 to 1 should be entered here.
	 * 0=least amount of saturation, 1=most.  .5 seems to be the best "standard"
	 * setting here.  .25 to .75 or maybe a bit less in 200-300 steps would
	 * probably be a good range for "user adjustment".
	 * This adjusts how much white is in the colours.
	 */
	/* double tint = 0.5; */

	/*
	 * Our brightness table.
	 * br[0] is for X0h colours   br[0][0] is for colour 00h, br[0][1] is for 10h, etc
	 * br[1] is for X1h thru XCh
	 * br[2] is for XDh
	 */
	static const double br[3][4] = {
		{ 0.50, 0.75, 1.00, 1.00 },
		{ 0.29, 0.45, 0.73, 0.90 },
		{ 0.00, 0.24, 0.47, 0.77 },
	};

	/*
	 * OK, this is what you were waiting for :-)  The action thingy that calcs
	 * our palette.  Ok, in the code, X is the "brightness bits" of the colour,
	 * and Z is our actual colour... i.e. XZh represents the NES colour value.
	 */
	for (int i = 0; i < 64; i++) {
		int x = i >> 4;
		int z = i & 0xf;
		unsigned int r, g, b;

		if (z > 0xd) {
			r = g = b = 0;
		} else if (z == 0xd) {
			r = g = b = component(br[2][x]);
		} else if (z == 0x0) {
			r = g = b = component(br[0][x]);
		} else {
			/*
			 * Convert Z to an angle in { 240, 210, ..., -90 },
			 * add hue argument, then convert to radians.
			 */
			double theta = M_PI * ((((9 - z) * 30) + hue) / 180.0);
			double sin_theta = tint * sin(theta);
			double cos_theta = tint * cos(theta);

			/*
			 * This next part is where the real work is done.  It is a mathematical
			 * representation of an NTSC chroma decoder matrix.  It converts Hue,
			 * Saturation, and Luminance to raw RGB values.
			 */
			double y = br[1][x];
			r = component(y + sin_theta);
			g = component(y - (27.0 / 53.0) * sin_theta + (10.0 / 53.0) * cos_theta);
			b = component(y - cos_theta);
		}
		outbuf[i] = (r << 16) | (g << 8) | b;
	}
}

#endif /* HAVE_LIBM */
