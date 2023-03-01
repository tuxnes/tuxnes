/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: Abstract renderer interface
 */

#ifndef RENDERER_H
#define RENDERER_H

#ifndef X_DISPLAY_MISSING
#define HAVE_X 1
#endif /* !defined(X_DISPLAY_MISSING) */

/* only the no-op renderer `none' is universally known */
extern int      InitDisplayNone(int argc, char **argv);
extern void     UpdateColorsNone(void);
extern void     UpdateDisplayNone(void);

struct Renderer {
	const char *name, *fullname;
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
	char       *geometry;
	char       *display_id;
	int        inroot;
	int        indexedcolor;
} renderer_config;

/* global renderer data */
extern struct RendererData {
	int        basetime;
	int        pause_display;
	int        halfspeed;
	int        doublespeed;
	int        desync;
	int        needsredraw;            /* Refresh screen display */
	int        redrawbackground;       /* Redraw tile background */
	int        redrawall;              /* Redraw all scanlines */
} renderer_data;

#define maxsize 4

#define tilecachedepth 25

extern int              magstep, scaler_magstep;
extern unsigned int     currentbgcolor, oldbgcolor;
extern unsigned char    palette_cache[tilecachedepth][32];

#endif
