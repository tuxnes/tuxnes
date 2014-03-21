/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * $Id: renderer.h,v 1.5 2001/04/11 21:45:47 tmmm Exp $
 *
 * Description: Abstract renderer interface
 */

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
     char *name, *fullname;
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
     int	sticky_keys;
     char	*geometry;
     char	*display_id;
} renderer_config;

/* global renderer data */
extern struct RendererData {
     int	basetime;
     int	pause_display;
} renderer_data;
