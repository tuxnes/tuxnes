/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: Abstract renderer interface
 */

#ifndef _RENDERER_H_
#define _RENDERER_H_

#ifndef X_DISPLAY_MISSING
#define HAVE_X 1
#endif /* !defined(X_DISPLAY_MISSING) */

#ifdef HAVE_LIBGII
#ifdef HAVE_GGI_GII_H
#ifdef HAVE_LIBGGI
#ifdef HAVE_GGI_GGI_H
#define HAVE_GGI 1
#endif
#endif
#endif
#endif

#ifdef HAVE_LIBW
#ifdef HAVE_WLIB_H
#define HAVE_W 1
#endif
#endif

/* only the no-op renderer `none' is universally known */
extern int	InitDisplayNone(int argc, char **argv);
extern void	UpdateColorsNone(void);
extern void	UpdateDisplayNone(void);

/* renderer flags */
#define RENDERER_OLD		1
#define RENDERER_DIFF		2

struct Renderer {
     const char *name, *fullname;
     int _flags;
     int (*InitDisplay)(int argc, char **argv);
     void (*UpdateDisplay)(void);
     void (*UpdateColors)(void);
};

/* the currently selected renderer */
extern struct Renderer *renderer;

/* table of renderers, terminated by { 0, 0, 0, 0, 0, 0 } */
extern struct Renderer renderers[];

/* global renderer parameters */
extern struct RendererConfig {
     int	inroot;
     char	*geometry;
     char	*display_id;
} renderer_config;

/* global renderer data */
extern struct RendererData {
     int	basetime;
     int	pause_display;
} renderer_data;

/* only 1x1 or 2x2 integer scaling is permitted for now */
#define maxsize 2

#define tilecachedepth 25

extern int	indexedcolor;
extern int	screenshotnumber;
extern char	*screenshotfile;
extern int	magstep;
extern int	halfspeed, doublespeed;
extern int	desync;
extern unsigned int	currentbgcolor, oldbgcolor;
extern unsigned char	needsredraw;		/* Refresh screen display */
extern unsigned char	redrawbackground;	/* Redraw tile background */
extern unsigned char	redrawall;		/* Redraw all scanlines */
extern unsigned char	palette_cache[tilecachedepth][32];

#endif
