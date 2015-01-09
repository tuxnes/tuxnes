/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: Framebuffer/pixmap rendering; the main rendering
 * function drawimage* is included (several times) from pixels.h.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_FEATURES_H
#include <features.h>
#endif

#include "globals.h"
#include "mapper.h"
#include "renderer.h"

/* globals */

unsigned int    nextline;
unsigned int	bpp;
unsigned int    bpu;
unsigned int    bitmap_pad;
unsigned int	depth;
unsigned int    bytes_per_line;
unsigned int	lsb_first;
unsigned int	lsn_first; /* nybbles swapped? Ick! */
unsigned int    pix_swab;
unsigned int	vline = 0;
unsigned int	vscan = 0;
unsigned int	frameskip = 0;
unsigned int	vwrap = 0;
unsigned int    scanpage = 0;
unsigned int    scanlines = 100;
char	*fb = 0;
char	*rfb = 0;
int	 palette_alloc[25];
int	*palette = palette_alloc;
int	 palette_alloc2[25];
int	*palette2 = palette_alloc2;

void	(*drawimage)(int);
void	fbinit(void);

void	mmc2_4_latch(int);
void	mmc2_4_latchspr(int);

#undef DOUBLE
#define DOUBLE 0

#undef SCANLINES
#define SCANLINES 0

#define BPP 1
#include "pixels.h"

#undef BPP

#define BPP 4
#include "pixels.h"

#undef BPP

#define BPP 8
#include "pixels.h"

#undef BPP

#define BPP 16
#include "pixels.h"

#undef BPP

#define BPP 24
#include "pixels.h"

#undef BPP

#define BPP 32
#include "pixels.h"

#undef BPP

#undef DOUBLE
#define DOUBLE 1

#undef SCANLINES
#define SCANLINES 0

#define BPP 1
#include "pixels.h"

#undef BPP

#define BPP 4
#include "pixels.h"

#undef BPP

#define BPP 8
#include "pixels.h"

#undef BPP

#define BPP 16
#include "pixels.h"

#undef BPP

#define BPP 24
#include "pixels.h"

#undef BPP

#define BPP 32
#include "pixels.h"

#undef BPP

#undef SCANLINES
#define SCANLINES 1

#define BPP 1
#include "pixels.h"

#undef BPP

#define BPP 4
#include "pixels.h"

#undef BPP

#define BPP 8
#include "pixels.h"

#undef BPP

#define BPP 16
#include "pixels.h"

#undef BPP

#define BPP 24
#include "pixels.h"

#undef BPP

#define BPP 32
#include "pixels.h"

#undef BPP

static void
drawimage_old(int endclock)
{
  return;
}

void
fbinit(void)
{
  /* this is the default inter-line skip */
  nextline = bytes_per_line;
  if ((renderer->_flags & RENDERER_OLD) ||
      (renderer->InitDisplay == InitDisplayNone))
    {
      /* Point drawimage to the no-op version */
      drawimage = drawimage_old;
    }
  else if (renderer->_flags & RENDERER_DIFF)
    {
      /* Current: Point drawimage to the 8bpp version */
      /* Ideal: for !indexedcolor, should be drawimage6 with nextline=192 */
      /*        for indexedcolor, should be drawimage5 with nextline=160 */
      nextline = 256;
      drawimage = drawimage8;
    }
  else
    {
      /* Point drawimage to the correct version for the bpp used: */
      if (bpp == 1)
	{
	  drawimage = (magstep == 2) ? (scanlines != 100) ? &drawimage1s : &drawimage1d : &drawimage1;
	}
      else if (bpp == 4)
	{
	  fprintf (stderr,
		   "======================================================\n"
		   "Warning: Using untested 4bpp rendering code\n"
		   "\n"
		   "This display code may be buggy or non-functional.\n"
		   "\n"
		   "In case of difficulty, switch to --renderer=diff\n"
		   "                              or --renderer=old\n"
		   "\n"
		   "Please send a status report to <tuxnes-dev@dod.hpi.net>.\n"
		   "Describe the appearance of TuxNES, and whether it is correct.\n"
		   "Also include this information in your report:\n"
		   "\n");
	  fprintf (stderr, "%s @ 4bpp\n",
		   renderer->fullname);
	  fprintf (stderr, PACKAGE_NAME " release: %s-%s\n", PACKAGE, VERSION);
	  fprintf (stderr, "Built on %s at %s\n", __DATE__, __TIME__);
	  fprintf (stderr, "\n");
	  fprintf (stderr, "OS version and platform:\n");
	  fflush (stderr);
	  system ("uname -a >&2");
	  fprintf (stderr, "======================================================\n");
	  fflush (stderr);
	  drawimage = (magstep == 2) ? (scanlines != 100) ? &drawimage4s : &drawimage4d : &drawimage4;
	}
      else if (bpp == 8)
	{
	  drawimage = (magstep == 2) ? (scanlines != 100) ? &drawimage8s : &drawimage8d : &drawimage8;
	}
      else if (bpp == 16)
	{
	  drawimage = (magstep == 2) ? (scanlines != 100) ? &drawimage16s : &drawimage16d : &drawimage16;
	  nextline = bytes_per_line / 2;
	}
      else if (bpp == 24)
	{
	  drawimage = (magstep == 2) ? (scanlines != 100) ? &drawimage24s : &drawimage24d : &drawimage24;
	}
      else if (bpp == 32)
	{
	  drawimage = (magstep == 2) ? (scanlines != 100) ? &drawimage32s : &drawimage32d : &drawimage32;
	  nextline = bytes_per_line / 4;
	}
      else
	{
	  fprintf (stderr, "Don't know how to handle %dbpp\n", bpp);
	  fprintf (stderr,
		   "As a temporary workaround, try --renderer=diff\n");
	  fprintf (stderr,
		   "                            or --renderer=old\n");
	  exit (1);
	}
      if ((bpp == 1) && (bpu > 8) && pix_swab)
	{
	  fprintf (stderr,
		   "Don't know how to handle pix_swabbed %dbpp/%dbpu\n",
		   bpp, bpu);
	  fprintf (stderr,
		   "As a temporary workaround, try --renderer=diff\n");
	  fprintf (stderr,
		   "                            or --renderer=old\n");
	  exit (1);
	}
    }
}
