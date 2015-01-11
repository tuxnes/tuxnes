/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: This file provides the "auto" and "none"
 * pseudo-renderers.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "consts.h"
#include "globals.h"
#include "joystick.h"
#include "mapper.h"
#include "renderer.h"
#include "sound.h"

#ifdef HAVE_X
extern int	InitDisplayX11(int argc, char **argv);
extern void	UpdateColorsX11(void);
extern void	UpdateDisplayX11(void);
extern void	UpdateDisplayOldX11(void);
#endif

#ifdef HAVE_GGI
extern int	InitDisplayGGI(int argc, char **argv);
extern void	UpdateColorsGGI(void);
extern void	UpdateDisplayGGI(void);
#endif

#ifdef HAVE_W
extern int	InitDisplayW(int argc, char **argv);
extern void	UpdateColorsW(void);
extern void	UpdateDisplayW(void);
#endif

/* exports */
int	InitDisplayAuto(int argc, char **argv);
int	InitDisplayNone(int argc, char **argv);
void	UpdateColorsNone(void);
void	UpdateDisplayNone(void);

/* imports */
extern void	fbinit(void);

/* globals */
struct Renderer renderers[] = {
#ifdef HAVE_X
     { "x11", "X11 renderer",
       0,
       InitDisplayX11, UpdateDisplayX11, UpdateColorsX11 },
     { "diff", "differential X11 renderer",
       RENDERER_DIFF,
       InitDisplayX11, UpdateDisplayX11, UpdateColorsX11 },
     { "old", "old X11 renderer (tile-based)",
       RENDERER_OLD,
       InitDisplayX11, UpdateDisplayOldX11, UpdateColorsX11 },
#endif /* HAVE_X */
#ifdef HAVE_GGI
     { "ggi", "GGI renderer",
       0,
       InitDisplayGGI, UpdateDisplayGGI, UpdateColorsGGI },
#endif /* HAVE_GGI */
#ifdef HAVE_W
     { "w", "W renderer",
       0,
       InitDisplayW, UpdateDisplayW, UpdateColorsW },
#endif /* HAVE_W */
     { "auto", "Choose one automatically",
       0,
       InitDisplayAuto, 0, 0 },
     { "none", "Don't draw anything",
       0,
       InitDisplayNone, UpdateDisplayNone, UpdateColorsNone },
     { 0, 0, 0, 0, 0, 0 }     /* terminator */
}, *renderer = 0;

struct RendererConfig renderer_config = {
     /* int	inroot */
     0,
     /* char	*geometry */
     0,
     /* char    *display_id */
     0
};

/* global renderer data */
struct RendererData renderer_data = {
     /* int	basetime */
     0,
     /* int	pause_display */
     0
};

int	indexedcolor = 1;
int	magstep = 0;
int	halfspeed = 0, doublespeed = 0;
int	desync = 1;
unsigned int	currentbgcolor, oldbgcolor;
unsigned char	needsredraw = 1;	/* Refresh screen display */
unsigned char	redrawbackground = 1;	/* Redraw tile background */
unsigned char	redrawall = 1;		/* Redraw all scanlines */
unsigned char	palette_cache[tilecachedepth][32];

int
InitDisplayAuto(int argc, char **argv)
{
     const char *rendname = 0;

#ifdef HAVE_W
     {
	  struct stat statbuf;
	  if (getenv ("WDISPLAY") ||
	      (! access ("/tmp/wserver", F_OK | W_OK) &&
	       (! stat ("/tmp/wserver", &statbuf)) &&
	       S_ISSOCK (statbuf.st_mode)))
	  {
	       rendname = "w";
	  }
     }
#endif
     if (getenv ("DISPLAY"))
     {
#ifdef HAVE_X
	  rendname = "x11";
#endif
#ifdef HAVE_GGI
	  if (! rendname)
	       rendname = "ggi";
#endif
#ifdef HAVE_W
	  if (! rendname)
	       rendname = "w";
#endif
     }
     else if (! rendname)
     {
#ifdef HAVE_GGI
	  rendname = "ggi";
#endif
#ifdef HAVE_W
	  if (! rendname)
	       rendname = "w";
#endif
     }
     if (! rendname)
     {
	  rendname = "none";
	  fprintf (stderr,
		   "======================================================\n"
		   "Warning: No Suitable Renderer Detected\n"
		   "\n"
		   "The `%s' fall-back renderer will be used.\n"
		   "\n"
		   "To avoid this in the future, install additional\n"
		   "libraries and recompile, or use the --renderer=...\n"
		   "option to specify a renderer explicitly.\n"
		   "\n"
		   "The option --help=options lists available renderers.\n"
		   "======================================================\n",
		   rendname);
     }
     for (renderer = renderers; renderer->name; renderer++)
	  if (! strcmp (renderer->name, rendname))
	       break;
     if (! renderer)
     {
	  fprintf (stderr, "%s: unrecognized renderer name `%s'\n",
		   *argv, rendname);
	  return 1;
     }
     return renderer->InitDisplay (argc, argv);
}

int
InitDisplayNone(int argc, char **argv)
{
     fbinit ();
     return 0;
}

void
UpdateDisplayNone(void)
{
  struct timeval time;
  static unsigned int frame;
  unsigned int timeframe;

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

  /* Slow down if we're getting ahead */
  if (frame > timeframe + 1 && frameskip == 0)
    {
      usleep (16666 * (frame - timeframe - 1));
    }

  /* Input loop */
  do {
    /* Handle joystick input */
    js_handle_input();
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

/* Update the colors on the screen if the palette changed */
void
UpdateColorsNone(void)
{
     /* no-op */
}
