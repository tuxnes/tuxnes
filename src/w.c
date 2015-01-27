/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: This file handles the I/O subsystem for the W window
 * system.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif /* HAVE_FEATRES_H */

#include <sys/time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__FreeBSD__)
#include <machine/endian.h>
#elif defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/endian.h>
#else /* Linux */
#include <endian.h>
#endif

#include "consts.h"
#include "controller.h"
#include "globals.h"
#include "joystick.h"
#include "mapper.h"
#include "renderer.h"
#include "screenshot.h"
#include "sound.h"

/* imports */
void	quit(void);
void	START(void);
void	fbinit(void);

#ifdef HAVE_W
#include <Wlib.h>

/* exported functions */
int	InitDisplayW(int argc, char **argv);
void	UpdateColorsW(void);
void	UpdateDisplayW(void);

static void	InitScreenshotW(void);
static void	SaveScreenshotW(void);
static void	HandleKeyboardW(WEVENT *);

static WSERVER *serverW;
static WWIN *winW, *iconW = 0;
static BITMAP *bitmapW;
static short width, height, x0, y0;
static short paletteW[64], palette2W[64];
static short whitepixel, blackpixel;

#endif

#ifdef HAVE_W

static void
InitScreenshotW(void)
{
  screenshot_init(".p8m");
}

static void
SaveScreenshotW(void)
{
  screenshot_new();
  if (! w_writepbm (screenshotfile, bitmapW))
    {
      if (verbose)
	{
	  fprintf (stderr, "Wrote screenshot to %s\n", screenshotfile);
	}
    }
  else
    {
      fprintf (stderr, "%s: w_writepbm failed\n", screenshotfile);
    }
}

static void
HandleKeyboardW(WEVENT *ev)
{
  _Bool press = ev -> type == EVENT_KEY;

  if (press && ev -> key == 27)
    {
      if (winW != WROOT) w_delete (winW);
      w_exit ();
      quit ();                    /* ESC */
    }

  /* the coin and dipswitch inputs work only in VS UniSystem mode */
  if (unisystem)
    switch (ev -> key)
      {
      case '[':
      case '{':
        ctl_coinslot(0x01, press);
        break;
      case ']':
      case '}':
        ctl_coinslot(0x02, press);
        break;
      case '\\':
      case '|':
        ctl_coinslot(0x04, press);
        break;
      case 'Q':
      case 'q':
        ctl_dipswitch(0x01, press);
        break;
      case 'W':
      case 'w':
        ctl_dipswitch(0x02, press);
        break;
      case 'E':
      case 'e':
        ctl_dipswitch(0x04, press);
        break;
      case 'R':
      case 'r':
        ctl_dipswitch(0x08, press);
        break;
      case 'T':
      case 't':
        ctl_dipswitch(0x10, press);
        break;
      case 'Y':
      case 'y':
        ctl_dipswitch(0x20, press);
        break;
      case 'U':
      case 'u':
        ctl_dipswitch(0x40, press);
        break;
      case 'I':
      case 'i':
        ctl_dipswitch(0x80, press);
        break;
      }

  switch (ev -> key)
    {
      /* controller 1 keyboard mapping */
    case '\r':
    case '\n':
      ctl_keypress(0, STARTBUTTON, press);
      break;
    case '\t':
      ctl_keypress(0, SELECTBUTTON, press);
      break;
    case WKEY_UP:
      ctl_keypress(0, UP, press);
      break;
    case WKEY_DOWN:
      ctl_keypress(0, DOWN, press);
      break;
    case WKEY_LEFT:
      ctl_keypress(0, LEFT, press);
      break;
    case WKEY_RIGHT:
      ctl_keypress(0, RIGHT, press);
      break;
    case WKEY_HOME:
      ctl_keypress_diag(0, UP | LEFT, press);
      break;
    case WKEY_PGUP:
      ctl_keypress_diag(0, UP | RIGHT, press);
      break;
    case WKEY_END:
      ctl_keypress_diag(0, DOWN | LEFT, press);
      break;
    case WKEY_PGDOWN:
      ctl_keypress_diag(0, DOWN | RIGHT, press);
      break;
    case 'Z':
    case 'z':
    case 'X':
    case 'x':
    case 'D':
    case 'd':
    case WKEY_INS:
    case WMOD_SHIFT | WMOD_LEFT:
    case WMOD_SHIFT | WMOD_RIGHT:
    case WMOD_CTRL | WMOD_LEFT:
    case WMOD_ALT | WMOD_RIGHT:
    case WMOD_META | WMOD_RIGHT:
    case WMOD_ALTGR | WMOD_LEFT:
    case WMOD_ALTGR | WMOD_RIGHT:
      ctl_keypress(0, BUTTONB, press);
      break;
    case 'A':
    case 'a':
    case 'C':
    case 'c':
    case ' ':
    case WKEY_DEL:
    case WMOD_ALT | WMOD_LEFT:
    case WMOD_CTRL | WMOD_RIGHT:
      ctl_keypress(0, BUTTONA, press);
      break;

      /* controller 2 keyboard mapping */
    case 'G':
    case 'g':
      ctl_keypress(1, STARTBUTTON, press);
      break;
    case 'F':
    case 'f':
      ctl_keypress(1, SELECTBUTTON, press);
      break;
    case 'K':
    case 'k':
      ctl_keypress(1, UP, press);
      break;
    case 'J':
    case 'j':
      ctl_keypress(1, DOWN, press);
      break;
    case 'H':
    case 'h':
      ctl_keypress(1, LEFT, press);
      break;
    case 'L':
    case 'l':
      ctl_keypress(1, RIGHT, press);
      break;
    case 'V':
    case 'v':
      ctl_keypress(1, BUTTONB, press);
      break;
    case 'B':
    case 'b':
      ctl_keypress(1, BUTTONA, press);
      break;
    }

  /* emulator keys */
  if (press)
    switch (ev -> key)
      {
/*       case WKEY_F1: */
/*         debug_bgoff = 1; */
/*         break; */
/*       case WKEY_F2: */
/*         debug_bgoff = 0; */
/*         break; */
/*       case WKEY_F3: */
/*         debug_spritesoff = 1; */
/*         break; */
/*       case WKEY_F4: */
/*         debug_spritesoff = 0; */
/*         break; */
/*       case WKEY_F5: */
/*         debug_switchtable = 1; */
/*         break; */
/*       case WKEY_F6: */
/*         debug_switchtable = 0; */
/*         break; */
      case WKEY_F7:
      case 'S':
      case 's':
        SaveScreenshotW ();
        break;
/*       case WKEY_F8: */
/*         memset (displaycachevalid, 0, sizeof(displaycachevalid)); */
/* 	memset (bitplanecachevalid, 0, sizeof(bitplanecachevalid)); */
/*         break; */
      case 8:
        START ();
        break;
      case 'P':
      case 'p':
	renderer_data.pause_display = ! renderer_data.pause_display;
	desync = 1;
	break;
      case '`':
        desync = 1;
        halfspeed = 1;
        doublespeed = 0;
        renderer_data.pause_display = 0;
        break;
      case '1':
        desync = 1;
        halfspeed = 0;
        doublespeed = 0;
        renderer_data.pause_display = 0;
        break;
      case '2':
        desync = 1;
        halfspeed = 0;
        doublespeed = 2;
        renderer_data.pause_display = 0;
        break;
      case '3':
        desync = 1;
        halfspeed = 0;
        doublespeed = 3;
        renderer_data.pause_display = 0;
        break;
      case '4':
        desync = 1;
        halfspeed = 0;
        doublespeed = 4;
        renderer_data.pause_display = 0;
        break;
      case '5':
        desync = 1;
        halfspeed = 0;
        doublespeed = 5;
        renderer_data.pause_display = 0;
        break;
      case '6':
        desync = 1;
        halfspeed = 0;
        doublespeed = 6;
        renderer_data.pause_display = 0;
        break;
      case '7':
        desync = 1;
        halfspeed = 0;
        doublespeed = 7;
        renderer_data.pause_display = 0;
        break;
      case '8':
        desync = 1;
        halfspeed = 0;
        doublespeed = 8;
        renderer_data.pause_display = 0;
        break;
      case '0':
        desync = 1;
        renderer_data.pause_display = 1;
        break;
      }
}

int
InitDisplayW(int argc, char **argv)
{
  int x;
  rgb_t color;
  struct timeval time;

  if (magstep < 1) {
    magstep = 1;
  }
  if (magstep > maxsize) {
    fprintf (stderr, "Warning: Enlargement factor %d is too large!\n",
	     magstep);
    magstep = maxsize;
  }
  if (! (serverW = w_init()))
    {
      fprintf (stderr, "%s: W initialization failed\n", argv[0]);
      exit (EXIT_FAILURE);
    }
  bpp = serverW -> planes;
  if (verbose)
    fprintf (stderr,
	     "W: %dbpp [%s, %s] %d shared colors, %s blocks\n",
	     bpp,
	     (serverW -> type == BM_PACKEDCOLORMONO) ? "bitplanes, monochrome"
	     : (serverW -> type == BM_PACKEDMONO) ? "1 bitplane, monochrome"
	     : (serverW -> type == BM_DIRECT8) ? "8-bit chunky (direct) colors"
	     : (serverW -> type == BM_PACKEDCOLOR) ? "bitplanes, colors settable"
	     : (serverW -> type == BM_DIRECT24) ? "24-bit (rgb) image"
	     : "unknown scheme",
	     (indexedcolor && (bpp > 1)) ? "dynamic" : "static",
	     serverW -> sharedcolors,
	     (serverW -> flags & WSERVER_SHM) ? "shared" : "normal");
  if (renderer_config.inroot)
    {
      width = serverW -> width;
      height = serverW -> height;
      x0 = 0;
      y0 = 0;
      winW = WROOT;
    }
  else
    {
      WFONT *fontW;

      width = UNDEF;
      height = UNDEF;
      x0 = UNDEF;
      y0 = UNDEF;
      if (renderer_config.geometry)
	{
	  char *s;
	  int c;

	  /* this little hack converts "NxM" geometry specs into "N,M" */
	  for (s = renderer_config.geometry; (c = *s); s++)
	    if ((c == 'x') || (c == ',')) {
	      *s = ',';
	      break;
	    }
	  scan_geometry (renderer_config.geometry,
			 &width, &height,
			 &x0, &y0);
	}
      if ((width == UNDEF) || ! width)
	width = 256 * magstep;
      if ((height == UNDEF) || ! height)
	height = 240 * magstep;
      if (! (fontW = w_loadfont ("lucidat", 13, F_BOLD)))
	if (! (fontW = w_loadfont (0, 0, 0)))
	  {
	    w_exit ();
	    fprintf (stderr, "%s: failed to acquire font\n", argv[0]);
	    exit (EXIT_FAILURE);
	  }
      if ((iconW = w_create (w_strlen (fontW, PACKAGE) + 4,
			     fontW -> height + 2,
			     W_MOVE | EV_MOUSE
			     | EV_KEYS | EV_MODIFIERS)))
	{
	  w_setfont (iconW, fontW);
	  w_printstring (iconW, 2, 1, PACKAGE);
	}
      w_unloadfont (fontW);
      if (! (winW = w_create (width, height,
			      W_MOVE | W_TITLE | W_CLOSE
			      | (iconW ? W_ICON : 0)
			      | W_RESIZE
			      | EV_KEYS | EV_MODIFIERS)))
	{
	  if (iconW) w_delete (iconW);
	  w_exit ();
	  fprintf (stderr, "%s: failed to create window\n", argv[0]);
	  exit (EXIT_FAILURE);
	}
      w_settitle (winW, PACKAGE_NAME);
      limit2screen (winW, &x0, &y0);
    }
  /* Allocate rendering bitmap */
  if (! (bitmapW = w_allocbm (256 * magstep, 240 * magstep,
			      serverW -> type,
			      ((serverW -> type == BM_DIRECT8) ||
			       (serverW -> type == BM_PACKEDCOLOR))
			      ? 1 << serverW -> planes
			      : 0)))
    {
      w_delete (winW); if (iconW) w_delete (iconW);
      w_exit ();
      fprintf (stderr, "%s: failed to allocate bitmap\n", argv[0]);
      exit (EXIT_FAILURE);
    }
  /* Allocate black and white pixels */
  for (blackpixel = 0; blackpixel < serverW -> sharedcolors; blackpixel ++)
    if (! w_getColor (winW, blackpixel,
		      &(color.red), &(color.green), &(color.blue)))
      {
	if ((color.red <= 0x0FU) &&
	    (color.green <= 0x0FU) &&
	    (color.blue <= 0x0FU))
	  break;
      }
  if (blackpixel == serverW -> sharedcolors)
    blackpixel = w_allocColor (winW, 0x00U, 0x00U, 0x00U);
  if (blackpixel < 0)
    {
      if (bpp > 1)
	fprintf (stderr,
		 "W: can't allocate black pixel -- using index 1\n");
      blackpixel = 1;
    }
  color.red = color.green = color.blue = 0x00U;
  if (bitmapW -> palette)
    bitmapW -> palette[blackpixel] = color;
  for (whitepixel = 0; whitepixel < serverW -> sharedcolors; whitepixel ++)
    if (! w_getColor (winW, whitepixel,
		      &(color.red), &(color.green), &(color.blue)))
      {
	if ((color.red >= 0xF0U) &&
	    (color.green >= 0xF0U) &&
	    (color.blue >= 0xF0U))
	  break;
      }
  if (whitepixel == serverW -> sharedcolors)
    whitepixel = w_allocColor (winW, 0xFFU, 0xFFU, 0xFFU);
  if (whitepixel < 0)
    {
      if (bpp > 1)
	fprintf (stderr,
		 "W: can't allocate white pixel -- using index 0\n");
      whitepixel = 0;
    }
  color.red = color.green = color.blue = 0xFFU;
  if (bitmapW -> palette)
    bitmapW -> palette[whitepixel] = color;
  for (x=0; x < 24; x++)
    {
      palette[x] = palette2[x] = whitepixel;
    }
  if (indexedcolor && (bpp > 1))
    {
      /* Pre-initialize the colormap to known values */
      oldbgcolor = currentbgcolor = NES_palette[0];
      color.red = ((NES_palette[0] & 0xFF0000) >> 16);
      color.green = ((NES_palette[0] & 0xFF00) >> 8);
      color.blue = (NES_palette[0] & 0xFF);
      for (x = 0; x <= 24; x++)
        {
          palette[x] =
	    w_allocColor (winW, color.red, color.green, color.blue);
          palette2[x] = blackpixel;
	  if (palette[x] < 0)
	    {
	      if (winW != WROOT) w_delete (winW); if (iconW) w_delete (iconW);
	      w_exit ();
	      fprintf (stderr, "Can't allocate colors!\n");
	      exit (EXIT_FAILURE);
	    }
	  else if (bitmapW -> palette)
	    bitmapW -> palette[palette[x]] = color;
        }
      if (scanlines && (scanlines != 100))
	{
	  fprintf (stderr, "Warning: Scanline intensity is ignored in indexed-color modes!\n");
	  scanlines = 0;
	}
    }
  else /* truecolor */
    {
      indexedcolor = 0;
      for (x=0; x < 64; x++)
	{
	  short pixel;
	  rgb_t desired;

	  desired.red = ((NES_palette[x] & 0xFF0000) >> 16);
	  desired.green = ((NES_palette[x] & 0xFF00) >> 8);
	  desired.blue = (NES_palette[x] & 0xFF);
	  for (pixel = 0; pixel < serverW -> sharedcolors; pixel ++)
	    if (! w_getColor (winW, pixel,
			      &(color.red), &(color.green), &(color.blue)))
	      {
		if ((color.red == desired.red) &&
		    (color.green == desired.green) &&
		    (color.blue == desired.blue))
		  break;
	      }
	  if (pixel == serverW -> sharedcolors)
	    {
              pixel =
		   w_allocColor (winW,
				 desired.red, desired.green, desired.blue);
	    }
	  if (pixel < 0)
	    {
	      if ((0.299 * desired.red +
		   0.587 * desired.green +
		   0.114 * desired.blue) >= 0x80U)
		pixel = whitepixel;
	      else
		pixel = blackpixel;
	      if (bpp > 1)
		fprintf (stderr,
			 "Can't allocate color %d, using %s!\n", x,
			 pixel == blackpixel ? "black" : "white");
	    }
	  else if (bitmapW -> palette)
	    bitmapW -> palette[pixel] = desired;
	  paletteW[x] = pixel;
	  palette2W[x] = blackpixel;
	}
      if (scanlines && (scanlines != 100))
	for (x = 0; x < 64; x++)
	  {
	    short pixel;
	    rgb_t desired;
	    unsigned long r, g, b;
	    
	    r = ((NES_palette[x] & 0xFF0000) >> 8) * (scanlines / 100.0);
	    if (r > 0xFFFF)
	      r = 0xFFFF;
	    desired.red = (r + 0x7F) >> 8;
	    g = (NES_palette[x] & 0xFF00) * (scanlines / 100.0);
	    if (g > 0xFFFF)
	      g = 0xFFFF;
	    desired.green = (g + 0x7F) >> 8;
	    b = ((NES_palette[x] & 0xFF) << 8) * (scanlines / 100.0);
	    if (b > 0xFFFF)
	      b = 0xFFFF;
	    desired.blue = (b + 0x7F) >> 8;
	    for (pixel = 0; pixel < serverW -> sharedcolors; pixel ++)
	      if (! w_getColor (winW, pixel,
				&(color.red), &(color.green), &(color.blue)))
		{
		  if ((color.red == desired.red) &&
		      (color.green == desired.green) &&
		      (color.blue == desired.blue))
		    break;
		}
	    if (pixel == serverW -> sharedcolors)
	      {
		pixel =
		     w_allocColor (winW,
				   desired.red, desired.green, desired.blue);
	      }
	    if (pixel < 0)
	      {
	      if ((0.299 * desired.red +
		   0.587 * desired.green +
		   0.114 * desired.blue) >= 0x80U)
		  pixel = whitepixel;
		else
		  pixel = blackpixel;
		if (bpp > 1)
		  fprintf (stderr,
			   "Can't allocate color extra %d, using %s!\n", x,
			   pixel == blackpixel ? "black" : "white");
	      }
	    else if (bitmapW -> palette)
	      bitmapW -> palette[pixel] = desired;
	    palette2W[x] = pixel;
	  }
    }
  if ((winW != WROOT) && w_open (winW, x0, y0))
    {
      w_delete (winW); if (iconW) w_delete (iconW);
      w_exit ();
      fprintf (stderr, "%s: failed to open window\n", argv[0]);
      exit (EXIT_FAILURE);
    }

  /** endianness **/
  /* the W Window System is defined to be big-byte-endian */
  /* it's also defined to be big-bit-endian */
  /* it will likely be big-nybble-endian if 4bpp is ever added */
#if BYTE_ORDER == BIG_ENDIAN
  pix_swab = 0;
#else
  pix_swab = 1;
#endif
  lsb_first = 0;
  lsn_first = lsb_first;

  /** bitmap format **/
  bpu = bitmapW -> unitsize;
  bytes_per_line = bpu * bitmapW -> upl;

  rfb = (fb = (char *) bitmapW -> data);
  InitScreenshotW ();
  fbinit ();
  w_setForegroundColor (winW, blackpixel);
  w_setmode (winW, M_DRAW);
  w_setlinewidth (winW, 1);
  w_pbox (winW, 0, 0, width, height);
  w_setBackgroundColor (winW, whitepixel);
  fbinit ();
  /* This hack properly initializes the bitmap */
  {
    int old_scanlines = scanlines;

    scanlines = 1;
    drawimage (PBL);
    UpdateColorsW();
    w_putblock (bitmapW, winW, 
		(256 * magstep - width) / -2,
		(240 * magstep - height) / -2);
    scanlines = old_scanlines;
  }
  w_flush ();
  gettimeofday (&time, NULL);
  renderer_data.basetime = time.tv_sec;
  return 0;
}

/* Update the colors on the screen if the palette changed */
void
UpdateColorsW(void)
{
/*  int x,y,t; */
  int x, c;
  rgb_t color;

  /* Set Background color */
  oldbgcolor = currentbgcolor;
  if (indexedcolor)
    {
      c = VRAM[0x3f00] & 63;
      currentbgcolor = NES_palette[c];
      if (currentbgcolor != oldbgcolor)
	{
	  color.red = ((currentbgcolor & 0xFF0000) >> 16);
	  color.green = ((currentbgcolor & 0xFF00) >> 8);
	  color.blue = (currentbgcolor & 0xFF);
	  if (w_changeColor(winW, palette[24],
			    color.red, color.green, color.blue)
	      < 0)
	    {
	      fprintf (stderr,
		       "W: w_changeColor failed for background color!\n");
	    }
	  else if (bitmapW -> palette)
	    bitmapW -> palette[palette[24]] = color;
	}
    }
  else
    /* truecolor */
    {
      palette[24] = currentbgcolor = paletteW[VRAM[0x3f00] & 63];
      if (scanlines && (scanlines != 100))
	palette2[24] = palette2W[VRAM[0x3f00] & 63];
      if (oldbgcolor != currentbgcolor)
	{
	  redrawbackground = 1;
	  needsredraw = 1;
	}
    }

  /* Tile colors */
  if (indexedcolor)
    {
      for (x = 0; x < 24; x++)
        {
	  c = VRAM[0x3f01 + x + (x / 3)] & 63;
          if (c != (palette_cache[0][1 + x + (x / 3)] & 63))
            {
              color.red = ((NES_palette[c] &
			    0xFF0000) >> 16);
              color.green = ((NES_palette[c] &
			      0xFF00) >> 8);
              color.blue = (NES_palette[c] &
			    0xFF);
	      if (w_changeColor(winW, palette[x],
				color.red, color.green, color.blue)
		  < 0)
		{
		  if (winW != WROOT) w_delete (winW); if (iconW) w_delete (iconW);
		  w_exit ();
		  fprintf (stderr,
			   "W: w_changeColor failed!\n");
		  exit (EXIT_FAILURE);
		}
	      else if (bitmapW -> palette)
		bitmapW -> palette[palette[x]] = color;
            }
        }
      memcpy (palette_cache[0], VRAM + 0x3f00, 32);
    }

  /* Set palette tables */
  if (indexedcolor)
    {
      /* Already done in InitDisplayW */
    }
  else
    /* truecolor */
    {
      for (x = 0; x < 24; x++)
	{
	  palette[x] = paletteW[VRAM[0x3f01 + x + (x / 3)] & 63];
	  if (scanlines && (scanlines != 100))
	    palette2[x] = palette2W[VRAM[0x3f01 + x + (x / 3)] & 63];
	}
    }
}

void
UpdateDisplayW(void)
{
  struct timeval time;
  static unsigned int frame;
  unsigned int timeframe;
  static int nodisplay = 0;

  /* do audio update */
  UpdateAudio();

  /* Check the time.  If we're getting behind, skip a frame to stay in sync. */
  gettimeofday (&time, NULL);
  timeframe = (time.tv_sec - renderer_data.basetime) * 50 + time.tv_usec / 20000;     /* PAL */
  timeframe = (time.tv_sec - renderer_data.basetime) * 60 + time.tv_usec / 16666;     /* NTSC */
  frame++;
  if (halfspeed)
    timeframe >>= 1;
  if (doublespeed == 2)
    timeframe <<= 1;
  else if (doublespeed > 2)
    timeframe *= doublespeed;
  if (desync)
    frame = timeframe;
  desync = 0;
  if (frame < timeframe - 20 && frame % 20 == 0)
    desync = 1;                 /* If we're more than 20 frames behind, might as well stop counting. */

  if (! nodisplay)
    {
      drawimage (PBL);
      if (!frameskip)
	{
	  UpdateColorsW();
	  w_putblock (bitmapW, winW, 
		      (256 * magstep - width) / -2,
		      (240 * magstep - height) / -2);
	  w_flush ();
	  redrawall = needsredraw = 0;
	}
    }

  needsredraw = 0;
  redrawbackground = 0;
  redrawall = 0;

  /* Slow down if we're getting ahead */
  if (frame > timeframe + 1 && frameskip == 0)
    {
      usleep (16666 * (frame - timeframe - 1));
    }

  /* Input loop */
  do {
    WEVENT *ev;

    /* Handle joystick input */
    js_handle_input();

    /* Handle W input */
    while ((ev = w_queryevent (0, 0, 0, 0))) {
      switch (ev -> type)
	{
	case EVENT_RESIZE:
	  width = ev -> w;
	  height = ev -> w;
	  w_resize (winW, width, height);
	  w_pbox (winW, 0, 0, width, height);
	  needsredraw = redrawall = 1;
	  break;
	case EVENT_KEY:
	case EVENT_KRELEASE:
	  HandleKeyboardW (ev);
	  break;
	case EVENT_GADGET:
	  switch (ev -> key)
	    {
	    case GADGET_CLOSE:
	      if (winW != WROOT) w_delete (winW); if (iconW) w_delete (iconW);
	    case GADGET_EXIT:
	      w_exit ();
	      quit ();
	      break;
	    case GADGET_ICON:
	      if (iconW && ! nodisplay)
		{
		  w_querywindowpos (winW, 1, &x0, &y0);
		  w_close (winW);
		  w_open (iconW, x0, y0);
		  nodisplay = 1;
		  needsredraw = redrawall = 0;
		  break;
		}
	      break;
	    }
	  break;
	case EVENT_MRELEASE:
	  if (nodisplay && iconW)
	    {
	      w_querywindowpos (iconW, 1, &x0, &y0);
	      w_close (iconW);
	      w_open (winW, x0, y0);
	      nodisplay = 0;
	      needsredraw = redrawall = 1;
	    }
	  break;
	}
    }
  } while (renderer_data.pause_display);
  
  /* Check the time.  If we're getting behind, skip next frame to stay in sync. */
  gettimeofday (&time, NULL);
  timeframe = (time.tv_sec - renderer_data.basetime) * 60 + time.tv_usec / 16666;     /* NTSC */
  if (halfspeed)
    timeframe >>= 1;
  if (doublespeed == 2)
    timeframe <<= 1;
  else if (doublespeed > 2)
    timeframe *= doublespeed;
  if (frame >= timeframe || frame % 20 == 0)
    frameskip = 0;
  else
    frameskip = 1;
}

#endif
