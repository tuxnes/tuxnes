/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: This file handles the I/O subsystem for the X window
 * system.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif /* HAVE_FEATRES_H */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
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

#ifdef HAVE_X

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#ifdef HAVE_X11_VROOT_H

/*
 * Get this from the xscreensaver package if you want support for virtual
 * root windows
 */

#include <X11/vroot.h>

#endif /* HAVE_X11_VROOT_H */

/* check for XPM support */
#ifdef HAVE_LIBXPM
#ifdef HAVE_X11_XPM_H

#include <X11/xpm.h>

#define HAVE_XPM 1

#endif /* HAVE_X11_XPM_H */
#endif /* HAVE_LIBXPM */

/* check for X Extension support */
#ifdef HAVE_LIBXEXT
#ifdef HAVE_X11_EXTENSIONS_XEXT_H

#include <X11/extensions/Xext.h>

#define HAVE_XEXT 1

#endif /* HAVE_X11_EXTENSIONS_XEXT_H */
#endif /* HAVE_LIBXEXT */

/* check for X Shared Memory extension */
#ifdef HAVE_XEXT
#ifdef HAVE_SYS_IPC_H
#ifdef HAVE_SYS_SHM_H
#ifdef HAVE_X11_EXTENSIONS_XSHM_H
#define HAVE_SHM 1
#endif /* HAVE_X11_EXTENSIONS_XSHM_H */
#endif /* HAVE_SYS_SHM_H */
#endif /* HAVE_SYS_IPC_H */
#endif /* HAVE_XEXT */

#if HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#ifndef X_ShmAttach
#include <X11/Xmd.h>
#include <X11/extensions/shmproto.h>
#endif
#endif

#endif /* HAVE_X */

#define ARRAY_LEN(x) (sizeof (x) / sizeof *(x))

#define screen_on (RAM[0x2001]&8)
#define sprites_on (RAM[0x2001]&16)

#ifdef HAVE_X

/* exported functions */
int     InitDisplayX11(int argc, char **argv);
void    UpdateColorsX11(void);
void    UpdateDisplayX11(void);
void    UpdateDisplayOldX11(void);

/* X11 stuff: */
static Display  *display;
static int      (*oldhandler)(Display *, XErrorEvent *) = 0;
static Visual   *visual;
static Window    rootwindow, w;
static int       screen;
static Colormap colormap;

static unsigned int paletteX11[64];
static unsigned int palette2X11[64];

static unsigned char    *keystate[32];
static GC       gc, blackgc;
static GC       addgc, maskgc;
static GC       backgroundgc, solidbggc;
static GC       color1gc, color2gc, color3gc, bgcolorgc;
static GC       spritesolidcolorgc, spritecolor1gc, spritecolor2gc, spritecolor3gc;
static GC       planegc, blackplanegc, planenorgc, planeandgc, planexorgc;
static int      black, white;
static XSizeHints       sizehints;
static XClassHint      classhints;
static XWMHints        wmhints;
static XGCValues        GCValues;
static unsigned long    colortableX11[25];
static XColor   color;

static Pixmap background[tilecachedepth];      /* The pre-rendered background that sprites are drawn on top of */
static Pixmap bgplane1[tilecachedepth], bgplane2[tilecachedepth], bgplane3[tilecachedepth],
  bgmask[tilecachedepth];       /* Bitmaps for each color plane of the rendered background */
static Pixmap tilecache1[tilecachedepth], tilecache2[tilecachedepth], tilecache3[tilecachedepth],
  tilebgmask[tilecachedepth];   /* Bitmaps for the background tiles in each of the four colors */
static Pixmap layout;                  /* To assemble the final image to be displayed */
static XImage   *tilebitimage, *tilebitimage2;
static unsigned char tilebitdata[256 * maxsize * 64 * maxsize];
static unsigned char tilebitdata2[256 * maxsize * 64 * maxsize];
static XImage   *image = 0;

/* X11 virtual framebuffer */
static char     *xfb = 0;

/* X11 differential framebuffers */
static char     *xdfb[2] = {0, 0};
static int      xdfb_frame = 0;

/* externel and forward declarations */
void    fbinit(void);
void    quit(void);
void    START(void);
static void     InitScreenshotX11(void);
static void     SaveScreenshotX11(void);
static void     DiffUpdateOldX11(void);
static void     DoBackgroundOldX11(void);
static void     DrawSpritesOldX11(void);
static int      InForegroundOldX11(unsigned int, unsigned int);
static void     HandleKeyboardX11(XEvent ev);
static void     LayoutBackgroundOldX11(void);
static void     UpdateTileColorsOldX11(void);
static void     UpdateTilesOldX11(void);

/* Flags for OldX11 differential renderer "old" */
static unsigned char    bgmask_changed[tilecachedepth];
static unsigned char    current_bgmask = 0;
static unsigned char    tilebgmask_changed[tilecachedepth];
static unsigned char    tilemask1_changed[tilecachedepth];
static unsigned char    tilemask2_changed[tilecachedepth];
static unsigned char    tilemask3_changed[tilecachedepth];
static unsigned char    scanline_diff[240];
static unsigned short int       tileline_begin[60];
static unsigned short int       tileline_end[60];

/* Precalculated/Cached values */
static unsigned char    spritecache[256];
static unsigned char    ntcache[tilecachedepth][3840];
static unsigned char    vramcache[tilecachedepth][4096];
static unsigned char    tiledirty[tilecachedepth][256];
static unsigned char    tilechanged[256];
static unsigned int     displaytilecache[tilecachedepth][3840];
static unsigned int     displaycolorcache[tilecachedepth][3840];
static unsigned char    displaycachevalid[tilecachedepth][3840];
static unsigned char    bitplanecachevalid[tilecachedepth][3840];
static unsigned int     bgcolor[tilecachedepth];

/* Misc globals */
static unsigned short int       oldhscroll[240], oldvscroll[240];
static unsigned int     currentline;
static int      debug_bgoff = 0;
static int      debug_spritesoff = 0;
static int      debug_switchtable = 0;
static int      currentcache = 0;
static int      nextcache = 1 % tilecachedepth;
static int width, height;

#if HAVE_SHM

static Status shm_status = 0;
static int shm_major, shm_minor;
static Bool shm_pixmaps;
static XShmSegmentInfo shminfo;
static Status shm_attached = 0;
static int shm_attaching = 0;
static XImage *shm_image = 0;

static void
cleanup_shm(void)
{
	if (shminfo.shmid >= 0) {
		if (shm_attached)
			XShmDetach(display, &shminfo);
		if (shm_image) {
			XDestroyImage(shm_image);
			shm_image = 0;
		}
		if (shminfo.shmaddr)
			shmdt(shminfo.shmaddr);
		shmctl(shminfo.shmid, IPC_RMID, 0);
		shminfo.shmid = -1;
	}
}

#endif

static void
InitScreenshotX11(void)
{
	screenshot_init(".xpm");
}

static void
SaveScreenshotX11(void)
{
#ifdef HAVE_XPM
	int status;

	screenshot_new();
	if (renderer->_flags & RENDERER_OLD) {
		if (depth == 1) {
			XImage *im;

			im = XGetImage(display, layout,
			                0, 0,
			                256 * magstep, 240 * magstep,
			                ~0, ZPixmap);
			for (int y = 0; y < 240 * magstep; y++)
				for (int x = 0; x < 256 * magstep; x++)
					XPutPixel(image, x, y, XGetPixel(im, x, y));
			status = XpmWriteFileFromImage(display, screenshotfile, image, NULL, NULL);
		} else {
			status = XpmWriteFileFromPixmap(display, screenshotfile, layout, 0, NULL);
		}
	} else {
		status =
		  XpmWriteFileFromImage(display, screenshotfile, image, NULL, NULL);
	}
	if (status == XpmSuccess) {
		if (verbose) {
			fprintf(stderr, "Wrote screenshot to %s\n", screenshotfile);
		}
	} else {
		fprintf(stderr, "%s: %s\n", screenshotfile, XpmGetErrorString(status));
	}
#else
	fprintf(stderr,
	        "cannot capture screenshots; please install Xpm library and then recompile\n");
#endif /* HAVE_XPM */
}

static int
handler(Display *display, XErrorEvent *ev)
{
#if HAVE_SHM
	if (shm_attaching
	 && (ev->error_code == BadAccess)
	/* && (ev->request_code == Find_the_request_code_for_MIT_SHM) */
	 && (ev->minor_code == X_ShmAttach)) {
		shm_attached = False;
		return 0;
	}
#endif
	if (oldhandler) {
		return oldhandler(display, ev);
	}
	return 0;
}

int
InitDisplayX11(int argc, char **argv)
{
	int x, y;
	int geometry_mask;
	int border;
	struct timeval time;
	char *wname[] = {
		PACKAGE_NAME,
		PACKAGE
	};
	XTextProperty name[2];

	display = XOpenDisplay(renderer_config.display_id);
	oldhandler = XSetErrorHandler(handler);
	if (display == NULL) {
		fprintf(stderr,
		        "%s: [%s] Can't open display: %s\n",
		        argv[0],
		        renderer->name,
		        XDisplayName(renderer_config.display_id));
		exit(EXIT_FAILURE);
	}
	screen = XDefaultScreen(display);
#ifdef HAVE_SHM
	shm_status = XShmQueryVersion(display, &shm_major, &shm_minor, &shm_pixmaps);
#endif /* HAVE_SHM */
	visual = XDefaultVisual(display, screen);
	rootwindow = RootWindow(display, screen);
	colormap = DefaultColormap(display, screen);
	white = WhitePixel(display, screen);
	black = BlackPixel(display, screen);
	depth = DefaultDepth(display, screen);
#if BYTE_ORDER == BIG_ENDIAN
	pix_swab = (ImageByteOrder(display) != MSBFirst);
#else
	pix_swab = (ImageByteOrder(display) == MSBFirst);
#endif
	lsb_first = (BitmapBitOrder(display) == LSBFirst);
	lsn_first = lsb_first; /* who knows? packed 4bpp is really an obscure case */
	bpu = BitmapUnit(display);
	bitmap_pad = BitmapPad(display);
	bpp = depth;
	if (depth > 1) {
		int formats = 0;
		XPixmapFormatValues *xpfv = XListPixmapFormats(display, &formats);

		for (int i = 0; i < formats; i++)
			if (xpfv[i].depth == depth) {
				bpp = xpfv[i].bits_per_pixel;
				break;
			}
	}
	if ((BYTE_ORDER != BIG_ENDIAN) && (BYTE_ORDER != LITTLE_ENDIAN)
	 && (bpp == 32)
	 && !renderer->_flags) {
		fprintf(stderr,
		        "======================================================\n"
		        "Warning: %s-endian Host Detected\n"
		        "\n"
		        "This host may not handle 32bpp display properly.\n"
		        "\n"
		        "In case of difficulty, switch to --renderer=diff\n"
		        "                              or --renderer=old\n"
		        "======================================================\n",
		        (BYTE_ORDER == PDP_ENDIAN) ? "PDP" : "Obscure");
		fflush(stderr);
	}
	if ((renderer->_flags & RENDERER_OLD) && (scanlines && (scanlines != 100))) {
		fprintf(stderr, "[%s] Scanline intensity is ignored by this renderer\n",
		        renderer->name);
		scanlines = 0;
	}
	if ((visual->class & 1) && indexedcolor) {
		if (XAllocColorCells(display, colormap, 0, 0, 0, colortableX11, 25) == 0) {
			fprintf(stderr, "%s: [%s] Can't allocate colors!\n",
			        *argv, renderer->name);
			exit(EXIT_FAILURE);
		}
		/* Pre-initialize the colormap to known values */
		color.red = ((NES_palette[0] & 0xFF0000) >> 8);
		color.green = (NES_palette[0] & 0xFF00);
		color.blue = ((NES_palette[0] & 0xFF) << 8);
		color.flags = DoRed | DoGreen | DoBlue;
		for (x = 0; x <= 24; x++) {
			color.pixel = colortableX11[x];
			if ((renderer->_flags & RENDERER_DIFF) && ((bpp != 8) || (magstep > 1)))
				palette[x] = x;
			else
				palette[x] = color.pixel;
			XStoreColor(display, colormap, &color);
			palette2[x] = BlackPixel(display, screen);
		}
		if (scanlines && (scanlines != 100)) {
			fprintf(stderr,
			        "[%s] Scanline intensity is ignored in indexed-color modes!\n",
			        renderer->name);
			scanlines = 0;
		}
	} else {
		indexedcolor = 0;
		/* convert palette to local color format */
		for (x = 0; x < 64; x++) {
			color.pixel = x;
			color.red = ((NES_palette[x] & 0xFF0000) >> 8);
			color.green = (NES_palette[x] & 0xFF00);
			color.blue = ((NES_palette[x] & 0xFF) << 8);
			color.flags = DoRed | DoGreen | DoBlue;
			if (XAllocColor(display, colormap, &color) == 0) {
				fprintf(stderr, "%s: [%s] Can't allocate colors!\n",
				        *argv, renderer->name);
				exit(EXIT_FAILURE);
			}
			paletteX11[x] = color.pixel;
			palette2X11[x] = BlackPixel(display, screen);
		}
		if (scanlines && (scanlines != 100))
			for (x = 0; x < 64; x++) {
				unsigned long r, g, b;

				color.pixel = x;
				r = ((NES_palette[x] & 0xFF0000) >> 8) * (scanlines / 100.0);
				if (r > 0xFFFF)
					r = 0xFFFF;
				color.red = r;
				g = (NES_palette[x] & 0xFF00) * (scanlines / 100.0);
				if (g > 0xFFFF)
					g = 0xFFFF;
				color.green = g;
				b = ((NES_palette[x] & 0xFF) << 8) * (scanlines / 100.0);
				if (b > 0xFFFF)
					b = 0xFFFF;
				color.blue = b;
				color.flags = DoRed | DoGreen | DoBlue;
				if (XAllocColor(display, colormap, &color) == 0) {
					fprintf(stderr, "[%s] Can't allocate extra colors!\n",
					        renderer->name);
					break;
				}
				palette2X11[x] = color.pixel;
			}
	}
	if (magstep < 1) {
		magstep = 1;
	}
	if ((magstep > maxsize) && !(renderer->_flags & RENDERER_DIFF)) {
		fprintf(stderr,
		        "[%s] Enlargement factor %d is too large!\n",
		        renderer->name, magstep);
		magstep = maxsize;
	}
	width = 256 * magstep;
	height = 240 * magstep;
	x = 0;
	y = 0;
	geometry_mask = HeightValue | WidthValue;
	if (renderer_config.geometry)
		geometry_mask |= XParseGeometry(renderer_config.geometry, &x, &y, &width, &height);
	if (renderer_config.inroot) {
		w = rootwindow;
	} else {
		XSetWindowAttributes attrs;

		attrs.backing_store = WhenMapped;
		attrs.cursor = XCreateFontCursor(display, XC_crosshair);
		w = XCreateWindow(display, rootwindow,
		                  x, y, width, height, 0,
		                  /* depth */ CopyFromParent,
		                  /* class */ InputOutput,
		                  /* visual */ visual,
		                  /* valuemask */
		                  CWBackingStore |
		                  CWCursor,
		                  /* attributes */ &attrs);
	}
	XGetGeometry(display, w, &rootwindow,
	             &x, &y, &width, &height, &border, &depth);
	gc = XCreateGC(display, w, 0, 0);
	blackgc = XCreateGC(display, w, 0, 0);
	GCValues.function = GXor;
	addgc = XCreateGC(display, w, GCFunction, &GCValues);
	GCValues.function = GXand;
	maskgc = XCreateGC(display, w, GCFunction, &GCValues);
	XSetForeground(display, gc, ~0UL);
	XSetBackground(display, gc, 0UL);
	XSetForeground(display, blackgc, 0);
	XSetBackground(display, blackgc, 0);
	GCValues.function = GXor;
	color1gc = XCreateGC(display, w, GCFunction, &GCValues);
	color2gc = XCreateGC(display, w, GCFunction, &GCValues);
	color3gc = XCreateGC(display, w, GCFunction, &GCValues);
	bgcolorgc = XCreateGC(display, w, GCFunction, &GCValues);
	XSetBackground(display, bgcolorgc, black);
	GCValues.function = GXcopy;
	spritesolidcolorgc = XCreateGC(display, w, GCFunction, &GCValues);
	spritecolor1gc = XCreateGC(display, w, GCFunction, &GCValues);
	spritecolor2gc = XCreateGC(display, w, GCFunction, &GCValues);
	spritecolor3gc = XCreateGC(display, w, GCFunction, &GCValues);

	if (!renderer_config.inroot) {
		/* set aspect ratio */
		/* sizehints.flags = PMinSize | PMaxSize | PResizeInc | PAspect | PBaseSize; */
		/*   sizehints.min_width = 256; */
		/*   sizehints.min_height = 240; */
		/*   sizehints.max_width = 256 * maxsize; */
		/*   sizehints.max_height = 240 * maxsize; */
		/*   sizehints.width_inc = 256; */
		/*   sizehints.height_inc = 240; */
		/*   sizehints.min_aspect.x = 256; */
		/*   sizehints.min_aspect.y = 240; */
		/*   sizehints.max_aspect.x = 256; */
		/*   sizehints.max_aspect.y = 240; */
		sizehints.flags = PBaseSize;
		sizehints.base_width = width;
		sizehints.base_height = height;
		XSetWMNormalHints(display, w, &sizehints);

		/* pass the command line to the window system */
		XSetCommand(display, w, argv, argc);

		/* set window manager hints */
		wmhints.flags = InputHint | StateHint;
		wmhints.input = True;
		wmhints.initial_state = NormalState;
		XSetWMHints(display, w, &wmhints);

		/* set window title */
		classhints.res_class = wname[0];
		classhints.res_name = wname[1];
		XSetClassHint(display, w, &classhints);
		if (!XStringListToTextProperty(wname, 1, name)) {
			fprintf(stderr, "[%s] Can't set window name property\n",
			        renderer->name);
		} else {
			XSetWMName(display, w, name);
		}
		if (!XStringListToTextProperty(wname + 1, 1, name + 1)) {
			fprintf(stderr, "[%s] Can't set icon name property\n",
			        renderer->name);
		} else {
			XSetWMIconName(display, w, name + 1);
		}
	}

	if (renderer->_flags & RENDERER_OLD) {
		for (x = 0; x < tilecachedepth; x++) {
			background[x] = XCreatePixmap(display, w, 512 * maxsize, 480 * maxsize, depth);
			bgplane1[x] = XCreatePixmap(display, w, 512 * maxsize, 480 * maxsize, 1);
			bgplane2[x] = XCreatePixmap(display, w, 512 * maxsize, 480 * maxsize, 1);
			bgplane3[x] = XCreatePixmap(display, w, 512 * maxsize, 480 * maxsize, 1);
			bgmask[x] = XCreatePixmap(display, w, 512 * maxsize, 480 * maxsize, 1);
			tilecache1[x] = XCreatePixmap(display, w, 256 * maxsize, 64 * maxsize, 1);
			tilecache2[x] = XCreatePixmap(display, w, 256 * maxsize, 64 * maxsize, 1);
			tilecache3[x] = XCreatePixmap(display, w, 256 * maxsize, 64 * maxsize, 1);
			tilebgmask[x] = XCreatePixmap(display, w, 256 * maxsize, 64 * maxsize, 1);
		}
		planegc = XCreateGC(display, tilebgmask[0], 0, 0);
		XSetForeground(display, planegc, 1);
		XSetBackground(display, planegc, 0);
		blackplanegc = XCreateGC(display, tilebgmask[0], 0, 0);
		XSetForeground(display, blackplanegc, 0);
		XSetBackground(display, blackplanegc, 0);
		GCValues.function = GXnor;
		planenorgc = XCreateGC(display, tilebgmask[0], GCFunction, &GCValues);
		GCValues.function = GXand;
		planeandgc = XCreateGC(display, tilebgmask[0], GCFunction, &GCValues);
		GCValues.function = GXxor;
		planexorgc = XCreateGC(display, tilebgmask[0], GCFunction, &GCValues);
	}
	backgroundgc = XCreateGC(display, w, 0, 0);
	XSetBackground(display, backgroundgc, black);
	solidbggc = XCreateGC(display, w, 0, 0);
	XSetBackground(display, solidbggc, black);
	tilebitimage = XCreateImage(display, visual, 1, XYBitmap, 0, tilebitdata,
	                            256 * maxsize, 64 * maxsize, 8, 32 * maxsize);
	tilebitimage2 = XCreateImage(display, visual, 1, XYBitmap, 0, tilebitdata2,
	                             256 * maxsize, 64 * maxsize, 8, 32 * maxsize);
	XMapWindow(display, w);
	XSelectInput(display, w, KeyPressMask | KeyReleaseMask | ExposureMask |
	             FocusChangeMask | KeymapStateMask | StructureNotifyMask);
	XFlush(display);
	for (y = 0; y < tilecachedepth; y++)
		for (x = 0; x < 256; x++)
			tiledirty[y][x] = 1;
	gettimeofday(&time, NULL);
	renderer_data.basetime = time.tv_sec;
	layout = XCreatePixmap(display, w, 256 * magstep, 240 * magstep, depth);
#ifdef HAVE_SHM
	if ((shm_status == True) && !(renderer->_flags & RENDERER_OLD)) {
		if (depth == 1)
			shm_image = XShmCreateImage(display, visual, depth,
			                         XYBitmap, NULL, &shminfo,
			                         256 * magstep, 240 * magstep);
		else
			shm_image = XShmCreateImage(display, visual, depth,
			                         ZPixmap, NULL, &shminfo,
			                         256 * magstep, 240 * magstep);
		if (shm_image) {
			bytes_per_line = shm_image->bytes_per_line;
			shminfo.shmid = -1;
			shminfo.shmid = shmget(IPC_PRIVATE, bytes_per_line * shm_image->height,
			                       IPC_CREAT|0777);
			if (shminfo.shmid < 0) {
				perror("shmget");
			} else {
				shminfo.shmaddr = 0;
				switch (fork()) {
				case -1:
					perror("fork");
					atexit(cleanup_shm);
				case 0:
					break;
				default:
					{
						int status = 0;

						atexit(cleanup_shm);
						while (wait(&status) != -1)
							if (WIFEXITED(status))
								exit(WEXITSTATUS(status));
							else if (WIFSIGNALED(status))
								raise(WTERMSIG(status));
						perror("wait");
						exit(EXIT_FAILURE);
					}
				}
				xfb =
				shminfo.shmaddr =
				shm_image->data = shmat(shminfo.shmid, 0, 0);
				shminfo.readOnly = False;
				XSync(display, False);
				shm_attaching = 1;
				shm_attached = XShmAttach(display, &shminfo);
				XSync(display, False);
				shm_attaching = 0;
				if (shm_attached)
					image = shm_image;
			}
		}
	}
#endif /* HAVE_SHM */
	if (verbose) {
		fprintf(stderr,
		        "[%s] %s visual, depth %d, %dbpp, %s colors, %s %s\n",
		        renderer->name,
		        (visual->class == StaticGray) ? "StaticGray" :
		        (visual->class == GrayScale) ? "GrayScale" :
		        (visual->class == StaticColor) ? "StaticColor" :
		        (visual->class == PseudoColor) ? "PseudoColor" :
		        (visual->class == TrueColor) ? "TrueColor" :
		        (visual->class == DirectColor) ? "DirectColor" :
		        "Unknown",
		        depth, bpp,
		        indexedcolor ? "dynamic" : "static",
		        image ? "shared-memory" : "plain",
		        (renderer->_flags & RENDERER_OLD) ? "Pixmap" : "XImage");
	}
	if (!image) {
		bytes_per_line = 256 / 8 * magstep * bpp;
		if (!(xfb = malloc(bytes_per_line * 240 * magstep))) {
			perror("malloc");
			exit(EXIT_FAILURE);
		}
		if (depth == 1)
			image = XCreateImage(display, visual, depth,
			                     XYBitmap, 0, xfb, 256 * magstep, 240 * magstep,
			                     8, bytes_per_line);
		else
			image = XCreateImage(display, visual, depth,
			                     ZPixmap, 0, xfb, 256 * magstep, 240 * magstep,
			                     bpp, bytes_per_line);
	}
	if (renderer->_flags & RENDERER_DIFF) {
		for (int f = 0; f < ARRAY_LEN(xdfb); f++) {
			if ((bpp == 8) && (f == 0) && (bytes_per_line == 256)) {
				xdfb[f] = xfb;
			} else {
				if (!(xdfb[f] = malloc((size_t)256 * 240))) {
					perror("malloc");
					exit(EXIT_FAILURE);
				}
				memset((void *)xdfb[f], 64, (size_t)256 * 240);
			}
		}
		xdfb_frame = 0;
		rfb = fb = xdfb[xdfb_frame];
	} else {
		rfb = fb = xfb;
	}
	XFillRectangle(display, w,
	               blackgc, 0, 0,
	               width, height);
	InitScreenshotX11();
	fbinit();
	return 0;
}

static void
HandleKeyboardX11(XEvent ev)
{
	KeySym keysym = XkbKeycodeToKeysym(display, ev.xkey.keycode, 0, 0);
	_Bool press = ev.type == KeyPress;

	if (press && keysym == XK_Escape)
		quit();                    /* ESC */

	/* the coin and dipswitch inputs work only in VS UniSystem mode */
	if (unisystem)
		switch (keysym) {
		case XK_bracketleft:
		case XK_braceleft:
			ctl_coinslot(0x01, press);
			break;
		case XK_bracketright:
		case XK_braceright:
			ctl_coinslot(0x02, press);
			break;
		case XK_backslash:
		case XK_bar:
			ctl_coinslot(0x04, press);
			break;
		case XK_Q:
		case XK_q:
			ctl_dipswitch(0x01, press);
			break;
		case XK_W:
		case XK_w:
			ctl_dipswitch(0x02, press);
			break;
		case XK_E:
		case XK_e:
			ctl_dipswitch(0x04, press);
			break;
		case XK_R:
		case XK_r:
			ctl_dipswitch(0x08, press);
			break;
		case XK_T:
		case XK_t:
			ctl_dipswitch(0x10, press);
			break;
		case XK_Y:
		case XK_y:
			ctl_dipswitch(0x20, press);
			break;
		case XK_U:
		case XK_u:
			ctl_dipswitch(0x40, press);
			break;
		case XK_I:
		case XK_i:
			ctl_dipswitch(0x80, press);
			break;
		}

	switch (keysym) {
		/* controller 1 keyboard mapping */
	case XK_Return:
	case XK_KP_Enter:
		ctl_keypress(0, STARTBUTTON, press);
		break;
	case XK_Tab:
	case XK_ISO_Left_Tab:
	case XK_KP_Add:
		ctl_keypress(0, SELECTBUTTON, press);
		break;
	case XK_Up:
	case XK_KP_Up:
	case XK_KP_8:
	case XK_Pointer_Up:
		ctl_keypress(0, UP, press);
		break;
	case XK_Down:
	case XK_KP_Down:
	case XK_KP_2:
	case XK_Pointer_Down:
		ctl_keypress(0, DOWN, press);
		break;
	case XK_Left:
	case XK_KP_Left:
	case XK_KP_4:
	case XK_Pointer_Left:
		ctl_keypress(0, LEFT, press);
		break;
	case XK_Right:
	case XK_KP_Right:
	case XK_KP_6:
	case XK_Pointer_Right:
		ctl_keypress(0, RIGHT, press);
		break;
	case XK_Pointer_UpLeft:
	case XK_Home:
	case XK_KP_Home:
	case XK_KP_7:
		ctl_keypress_diag(0, UP | LEFT, press);
		break;
	case XK_Pointer_UpRight:
	case XK_Prior:
	case XK_KP_Prior:
	case XK_KP_9:
		ctl_keypress_diag(0, UP | RIGHT, press);
		break;
	case XK_Pointer_DownLeft:
	case XK_End:
	case XK_KP_End:
	case XK_KP_1:
		ctl_keypress_diag(0, DOWN | LEFT, press);
		break;
	case XK_Pointer_DownRight:
	case XK_Next:
	case XK_KP_Next:
	case XK_KP_3:
		ctl_keypress_diag(0, DOWN | RIGHT, press);
		break;
	case XK_Z:
	case XK_z:
	case XK_X:
	case XK_x:
	case XK_D:
	case XK_d:
	case XK_Insert:
	case XK_KP_Insert:
	case XK_KP_0:
	case XK_Shift_L:
	case XK_Shift_R:
	case XK_Eisu_Shift:
	case XK_Control_L:
	case XK_Alt_R:
		ctl_keypress(0, BUTTONB, press);
		break;
	case XK_A:
	case XK_a:
	case XK_C:
	case XK_c:
	case XK_space:
	case XK_KP_Space:
	case XK_Clear:
	case XK_Delete:
	case XK_KP_Delete:
	case XK_KP_Begin:
	case XK_KP_5:
	case XK_KP_Decimal:
	case XK_Alt_L:
	case XK_Control_R:
		ctl_keypress(0, BUTTONA, press);
		break;

		/* controller 2 keyboard mapping */
	case XK_G:
	case XK_g:
		ctl_keypress(1, STARTBUTTON, press);
		break;
	case XK_F:
	case XK_f:
		ctl_keypress(1, SELECTBUTTON, press);
		break;
	case XK_K:
	case XK_k:
		ctl_keypress(1, UP, press);
		break;
	case XK_J:
	case XK_j:
		ctl_keypress(1, DOWN, press);
		break;
	case XK_H:
	case XK_h:
		ctl_keypress(1, LEFT, press);
		break;
	case XK_L:
	case XK_l:
		ctl_keypress(1, RIGHT, press);
		break;
	case XK_V:
	case XK_v:
		ctl_keypress(1, BUTTONB, press);
		break;
	case XK_B:
	case XK_b:
		ctl_keypress(1, BUTTONA, press);
		break;
	}

	/* emulator keys */
	if (press)
		switch (keysym) {
		case XK_F1:
			debug_bgoff = 1;
			break;
		case XK_F2:
			debug_bgoff = 0;
			break;
		case XK_F3:
			debug_spritesoff = 1;
			break;
		case XK_F4:
			debug_spritesoff = 0;
			break;
		case XK_F5:
			debug_switchtable = 1;
			break;
		case XK_F6:
			debug_switchtable = 0;
			break;
		case XK_F7:
		case XK_Print:
		case XK_S:
		case XK_s:
			SaveScreenshotX11();
			break;
		case XK_F8:
			memset(displaycachevalid, 0, sizeof displaycachevalid);
			memset(bitplanecachevalid, 0, sizeof bitplanecachevalid);
			break;
		case XK_BackSpace:
			START();
			break;
		case XK_Pause:
		case XK_P:
		case XK_p:
			renderer_data.pause_display = !renderer_data.pause_display;
			desync = 1;
			break;
		case XK_grave:
			desync = 1;
			halfspeed = 1;
			doublespeed = 0;
			renderer_data.pause_display = 0;
			break;
		case XK_1:
			desync = 1;
			halfspeed = 0;
			doublespeed = 0;
			renderer_data.pause_display = 0;
			break;
		case XK_2:
			desync = 1;
			halfspeed = 0;
			doublespeed = 2;
			renderer_data.pause_display = 0;
			break;
		case XK_3:
			desync = 1;
			halfspeed = 0;
			doublespeed = 3;
			renderer_data.pause_display = 0;
			break;
		case XK_4:
			desync = 1;
			halfspeed = 0;
			doublespeed = 4;
			renderer_data.pause_display = 0;
			break;
		case XK_5:
			desync = 1;
			halfspeed = 0;
			doublespeed = 5;
			renderer_data.pause_display = 0;
			break;
		case XK_6:
			desync = 1;
			halfspeed = 0;
			doublespeed = 6;
			renderer_data.pause_display = 0;
			break;
		case XK_7:
			desync = 1;
			halfspeed = 0;
			doublespeed = 7;
			renderer_data.pause_display = 0;
			break;
		case XK_8:
			desync = 1;
			halfspeed = 0;
			doublespeed = 8;
			renderer_data.pause_display = 0;
			break;
		case XK_0:
			desync = 1;
			renderer_data.pause_display = 1;
			break;
		}
}

void
UpdateDisplayX11(void)
{
	static XEvent ev;
	struct timeval time;
	static unsigned int frame;
	unsigned int timeframe;
	static int sleep = 0;         /* Initially we start with the emulation running.  If you want to wait until the window receives input focus, change this. */
	static int nodisplay = 0;

	/* do audio update */
	UpdateAudio();

	/* Check the time.  If we're getting behind, skip a frame to stay in sync. */
	gettimeofday(&time, NULL);
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

	if (!nodisplay) {
		drawimage(PBL);
		if (!frameskip) {
			int r = 0, r0 = 0;
			int y = 0;

			UpdateColorsX11();
			/* differential X11 renderer "diff" */
			if (renderer->_flags & RENDERER_DIFF) {
				int next_frame;
				static int virgin = 1;

				next_frame = (xdfb_frame + 1) % ARRAY_LEN(xdfb);
				/* This horrible hack checks for updates one scanline at a time. */
				/* Obviously, that only works well for some programs... */
				for (y = 0; y < 240; y++)
					if (virgin
					 || memcmp((void *)fb + (y << 8),
					           (void *)xdfb[next_frame] + (y << 8),
					           (size_t)256)) {
						if ((bpp != 8) || (magstep > 1)) {
							for (int x = 0; x < 256; x++) {
								int c = fb[(y << 8) + x];
								int c1 =
								  indexedcolor
								  ? colortableX11[c]
								  : paletteX11[c];
								int c2 =
								  (scanlines == 0)
								  ? black
								  : (scanlines == 100)
								  ? c1
								  : palette2X11[c];
								if (magstep == 1) {
									XPutPixel(image,
									          x * magstep, y * magstep,
									          c1);
								} else {
									for (int xxx = 0; xxx < magstep; xxx++) {
										XPutPixel(image,
										          x * magstep + xxx,
										          y * magstep,
										          c1);
										if (scanlines || virgin) {
											for (int yyy = 1; yyy < magstep; yyy++)
												XPutPixel(image,
												          x * magstep + xxx,
												          y * magstep + yyy,
												          c2);
										}
									}
								}
							}
						}
						else if (xdfb_frame || (bytes_per_line != 256)) {
							memcpy((void *)xfb + y * bytes_per_line,
							       (void *)xdfb[xdfb_frame] + (y << 8),
							       (size_t)256);
						}
						r++;
					} else if (r) {
						r0 += r;
#if HAVE_SHM
						if (shm_attached) {
							XShmPutImage(display, w, gc, image, 0, (y - r) * magstep,
							             (256 * magstep - width) / -2,
							             ((240 - 2 * (y - r)) * magstep - height) / -2,
							             256 * magstep, r * magstep,
							             True);
						} else
#endif
						{
							XPutImage(display, w, gc, image, 0, (y - r) * magstep,
							          (256 * magstep - width) / -2,
							          ((240 - 2 * (y - r)) * magstep - height) / -2,
							          256 * magstep, r * magstep);
						}
						r = 0;
					}
				virgin = 0;
				xdfb_frame = next_frame;
				rfb = fb = xdfb[xdfb_frame];
			} else {
				/* new X11 renderer "x11" */
				r = 240;
				y += r;
			}
			if (r) {
				r0 += r;
#if HAVE_SHM
				if (shm_attached) {
					XShmPutImage(display, w, gc, image, 0, (y - r) * magstep,
					             (256 * magstep - width) / -2,
					             ((240 - 2 * (y - r)) * magstep - height) / -2,
					             256 * magstep, r * magstep,
					             True);
				} else
#endif
				{
					XPutImage(display, w, gc, image, 0, (y - r) * magstep,
					          (256 * magstep - width) / -2,
					          ((240 - 2 * (y - r)) * magstep - height) / -2,
					          256 * magstep, r * magstep);
				}
			} if (r0) {
#if HAVE_SHM
				if (shm_attached) {
					/* hang the event loop until we get a ShmCompletion */
					ev.type = -1;
				} else
#endif
				{
					XFlush(display);
				}
			}
			redrawall = needsredraw = 0;
		}
	}

	/* Slow down if we're getting ahead */
	if (frame > timeframe + 1 && frameskip == 0) {
		usleep(16666 * (frame - timeframe - 1));
	}

	/* Input loop */
	do {
		/* Handle joystick input */
		js_handle_input();

		/* Handle X input */
		while (XPending(display) || ev.type == -1) {
			XNextEvent(display, &ev);
			/*printf("event %d\n", ev.type); */
			if (ev.type == DestroyNotify)
				quit();
			/* if you'd like defocusing to pause emulation, define this */
#ifdef PAUSE_ON_DEFOCUS
			if (ev.type == FocusIn)
				sleep = 0;
			if (ev.type == FocusOut)
				sleep = desync = 1;
#endif
			if (ev.type == MapNotify) {
				XExposeEvent *xev = (XExposeEvent *)&ev;

				xev->type = Expose;
				xev->count = 0;
				nodisplay = 0;
				needsredraw = redrawall = 1;
			} else if (ev.type == UnmapNotify) {
				nodisplay = 1;
				needsredraw = redrawall = 0;
			}
			/* Handle keyboard input */
			if (ev.type == KeyPress || ev.type == KeyRelease) {
				HandleKeyboardX11(ev);
			}
			if (ev.type == KeymapNotify) {
				memcpy(keystate, ((XKeymapEvent *)&ev)->key_vector, 32);
				controller[0] = controller[1] = 0;
				controllerd[0] = controllerd[1] = 0;
			}
			if (ev.type == ConfigureNotify) {
				XConfigureEvent *ce = (XConfigureEvent *)&ev;

				if ((ce->width != width)
				 || (ce->height != height)) {
					width = ce->width;
					height = ce->height;
					XFillRectangle(display, w,
					               blackgc, 0, 0,
					               width, height);
				}
			}
			if (ev.type == Expose) {
				XExposeEvent *xev = (XExposeEvent *)&ev;

				if (!xev->count) {
#if HAVE_SHM
					if (shm_attached) {
						XShmPutImage(display, w, gc, image, 0, 0,
						             (256 * magstep - width) / -2,
						             (240 * magstep - height) / -2,
						             256 * magstep, 240 * magstep,
						             True);
						/* hang the event loop until we get a ShmCompletion */
						ev.type = -1;
					} else
#endif
					{
						XPutImage(display, w, gc, image, 0, 0,
						          (256 * magstep - width) / -2,
						          (240 * magstep - height) / -2,
						          256 * magstep, 240 * magstep);
						XFlush(display);
					}
				}
			}
			if ((sleep || renderer_data.pause_display) && needsredraw) {
				XCopyArea(display, layout, w, gc,
				          0, 0,
				          256 * magstep, 240 * magstep,
				          (256 * magstep - width) / -2,
				          (240 * magstep - height) / -2);
				needsredraw = 0;
			}
		}
	} while (sleep || renderer_data.pause_display);

	needsredraw = 0;
	redrawbackground = 0;
	redrawall = 0;

	/* Check the time.  If we're getting behind, skip next frame to stay in sync. */
	gettimeofday(&time, NULL);
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

/*
   The screen draw works as follows:

   The full 4-screen background is laid out in Pixmap background.  The
   current window (h/v scrolling area) is copied into Pixmap layout.
   Sprites are then drawn on top of this, and finally the completed pixmap
   is copied onto the window.

   The final image is prepared in a pixmap before copying onto the screen
   because flicker would result if the sprites were drawn/erased directly
   onto the exposed window.  Therefore, backing store need not be
   maintained, as obscured areas can be recopied as necessary in response
   to expose events.
 */

void
UpdateDisplayOldX11(void)
{
	static XEvent ev;
	struct timeval time;
	static unsigned int frame;
	unsigned int timeframe;
	static int sleep = 0;         /* Initially we start with the emulation running.  If you want to wait until the window receives input focus, change this. */
	static int nodisplay = 0;

	/* do audio update */
	UpdateAudio();

	/* Check the time.  If we're getting behind, skip a frame to stay in sync. */
	gettimeofday(&time, NULL);
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

	if (!nodisplay) {
		if (frame >= timeframe || frame % 20 == 0) {
			/* If mode settings are different, force a redraw. */
			DiffUpdateOldX11();

			/* If the palette changed, update the colors. */
			UpdateColorsX11();

			/* Layout the background with h/v-scrolling */
			LayoutBackgroundOldX11();

			/* Draw the sprites on top */
			if (screen_on && sprites_on)
				DrawSpritesOldX11();
		}
	}

	/* Slow down if we're getting ahead */
	if (frame > timeframe + 1) {
		usleep(16666 * (frame - timeframe - 1));
	}

	/* Input loop */
	do {
		/* Handle joystick input */
		js_handle_input();

		/* Handle X input */
		while (XPending(display) || ev.type == -1) {
			XNextEvent(display, &ev);
			/*printf("event %d\n", ev.type); */
			if (ev.type == DestroyNotify)
				quit();
			/* if you'd like defocusing to pause emulation, enable this */
#ifdef PAUSE_ON_DEFOCUS
			if (ev.type == FocusIn)
				sleep = 0;
			if (ev.type == FocusOut)
				sleep = desync = 1;
#endif
			if (ev.type == MapNotify) {
				nodisplay = 0;
				needsredraw = redrawall = 1;
			} else if (ev.type == UnmapNotify) {
				nodisplay = 1;
				needsredraw = redrawall = 0;
			}

			/* Handle keyboard input */
			if (ev.type == KeyPress || ev.type == KeyRelease) {
				HandleKeyboardX11(ev);
			}
			if (ev.type == KeymapNotify) {
				memcpy(keystate, ((XKeymapEvent *)&ev)->key_vector, 32);
				controller[0] = controller[1] = 0;
				controllerd[0] = controllerd[1] = 0;
			}
			if (ev.type == ConfigureNotify) {
				XConfigureEvent *ce = (XConfigureEvent *)&ev;

				if ((ce->width != width)
				 || (ce->height != height)) {
					width = ce->width;
					height = ce->height;
					XFillRectangle(display, w,
					               blackgc, 0, 0,
					               width, height);
				}
			}
			if (ev.type == Expose)
				needsredraw = 1;
			if ((sleep || renderer_data.pause_display) && needsredraw) {
				XCopyArea(display, layout, w, gc,
				          0, 0,
				          256 * magstep, 240 * magstep,
				          (256 * magstep - width) / -2,
				          (240 * magstep - height) / -2);
				needsredraw = 0;
			}
		}
	} while (sleep || renderer_data.pause_display);

	if (needsredraw) {
		XCopyArea(display, layout, w, gc,
		          0, 0,
		          256 * magstep, 240 * magstep,
		          (256 * magstep - width) / -2,
		          (240 * magstep - height) / -2);
		XFlush(display);
		/*
		   The purpose of this is to wait for the acknowledgement from the
		   X server (a NoExpose event on the window background in response
		   to the CopyArea) before proceeding, so we don't send commands
		   faster than the X server can process them.  The -1 acts as a flag
		   to indicate that the XEvent loop should block.
		 */
		ev.type = -1;
	}

	XFlush(display);
	needsredraw = 0;
	redrawbackground = 0;
	redrawall = 0;
}

/* Update the background as necessary */
static void
DoBackgroundOldX11(void)
{
	unsigned int x, y, z;
	unsigned int h, v;
	int c;
	unsigned char *vramgroupptr, *cachegroupptr;
	unsigned char *colorptr;
	unsigned int tilecolor;
	unsigned int color1 = 0, color2 = 0, color3 = 0;
	static int count = 0;
	Pixmap plane1, plane2, plane3, bgplane;
/*	int hfirst, hlast, vfirst, vlast; */

	/* This can be used to skip frames for testing purposes */
	/*count++; if (count<100) return; */
	count = 0;

	/* Some games use the mapper to switch the graphic tiles on each frame,
	   so all are cached so that they can be switched quickly.  */
	plane1 = bgplane1[currentcache];
	plane2 = bgplane2[currentcache];
	plane3 = bgplane3[currentcache];
	bgplane = bgmask[currentcache];

	/* Go thru all rows and render any exposed tiles */
	for (y = 0; y < 60; y += 2) {
		c = ((tileline_end[y] - tileline_begin[y]) >> 4);
		if (c < 0)
			c = 0;
		else if (c < 16) {
			fprintf(stderr, "Error: Short scanline!\n");
			exit(EXIT_FAILURE);
		}                       /* Sanity check */
		else if (c >= 32)
			c = 32;
		else if (tileline_end[y] & 15)
			c++;
		for (x = h = (tileline_begin[y] >> 3) & 62; c--; x = (x + 2) & 62) {
			if (h > 63)
				exit(EXIT_FAILURE);  /* shouldn't happen */
			v = y;

			/* Colors are assigned to a group of four tiles, so we use
			   "groupptr" to point to this group. */
			if (nomirror) {
				vramgroupptr = VRAM + 0x2000 + ((x & 32) << 5) + ((v & ~1) % 30) * 32 + (x & 30) + (0x800 * (v >= 30));
				colorptr = VRAM + 0x23C0 + ((x & 32) << 5) + ((v % 30) & 28) *
				  2 + ((x & 28) >> 2) + (0x800 * (v >= 30));
			} else if (hvmirror == 0) {
				vramgroupptr = VRAM + 0x2000 + ((x & 32) << 5) + ((v & ~1) % 30) * 32 + (x & 30);
				colorptr = VRAM + 0x23C0 + ((x & 32) << 5) + ((v % 30) & 28) * 2 + ((x & 28) >> 2);
			} else {
				vramgroupptr = VRAM + ((0x2000 + ((v & ~1) % 30) * 32 + (x & 30)) ^ (0x400 * (v >= 30)));
				colorptr = VRAM + ((0x23C0 + ((v % 30) & 28) * 2 + ((x & 28) >> 2)) ^ (0x400 * (v >= 30)));
			}
			cachegroupptr = ntcache[currentcache] + (v & ~1) * 64 + (x & ~1);
			tilecolor = ((*colorptr) >> ((((v % 30) & 2) << 1) | (x & 2))) & 3;

			if (tilecolor != displaycolorcache[currentcache][(v & ~1) * 32 +
			                                                 (x >> 1)]) {
				displaycachevalid[currentcache][(v & ~1) * 64 + (x & ~1)] =
				displaycachevalid[currentcache][(v & ~1) * 64 + (x | 1)] =
				displaycachevalid[currentcache][(v | 1) * 64 + (x & ~1)] =
				displaycachevalid[currentcache][(v | 1) * 64 + (x | 1)] = 0;
			}

			/* When a new tile is exposed, first check to see if it's already
			   on the screen.  If it is, then just recopy it.  If not, then we
			   must go thru the (relatively slow) process of building it from
			   the bitplanes and colormaps.  Since a group of 4 tiles share the
			   same colormap, we do all four at once to keep things simpler. */

			if ((!displaycachevalid[currentcache][v * 64 + x])
			 || cachegroupptr[0] != vramgroupptr[0]
			 || cachegroupptr[1] != vramgroupptr[1]
			 || cachegroupptr[64] != vramgroupptr[32]
			 || cachegroupptr[65] != vramgroupptr[33]) {
				/* This only checks tiles in the same row.  The efficiency could be
				   improved by copying larger areas at once when possible. */
				for (z = 0; z < 64; z += 2)
					if (displaytilecache[currentcache][(v & ~1) * 32 + (z >> 1)]
					    == ((vramgroupptr[0] << 24) | (vramgroupptr[1] << 16) |
					        (vramgroupptr[32] << 8) | (vramgroupptr[33]))
					    && displaycolorcache[currentcache][(v & ~1) * 32 + (z >> 1)] == tilecolor
					    && displaycachevalid[currentcache][(v & ~1) * 64 + z]
					    && displaycachevalid[currentcache][(v & ~1) * 64 + z + 1]
					    && displaycachevalid[currentcache][(v | 1) * 64 + z]
					    && displaycachevalid[currentcache][(v | 1) * 64 + z + 1]
					    && z != (x & ~1))
						break;

				if (z < 64) {
					count++;      /* For profiling/debugging */
					XCopyArea(display, background[currentcache],
					          background[currentcache], gc, z * 8 * magstep,
					          (v & ~1) * 8 * magstep, 16 * magstep, 16 *
					 magstep, (x & ~1) * 8 * magstep, (v & ~1) * 8 * magstep);
					/* If only the color attributes changed, then the bitmaps do not need to be redrawn */
					if (cachegroupptr[0] != vramgroupptr[0]
					 || cachegroupptr[1] != vramgroupptr[1]
					 || cachegroupptr[64] != vramgroupptr[32]
					 || cachegroupptr[65] != vramgroupptr[33]
					 || (!bitplanecachevalid[currentcache][(v & ~1) * 64 + (x & ~1)])
					 || (!bitplanecachevalid[currentcache][(v | 1) * 64 + (x | 1)])
					 || (!bitplanecachevalid[currentcache][(v & ~1) * 64 + (x & ~1)])
					 || (!bitplanecachevalid[currentcache][(v | 1) * 64 + (x | 1)])) {
						XCopyArea(display, plane1, plane1, planegc,
						          z * 8 * magstep, (v & ~1) * 8 * magstep,
						          16 * magstep, 16 * magstep,
						          (x & ~1) * 8 * magstep, (v & ~1) * 8 * magstep);
						XCopyArea(display, plane2, plane2, planegc,
						          z * 8 * magstep, (v & ~1) * 8 * magstep,
						          16 * magstep, 16 * magstep,
						          (x & ~1) * 8 * magstep, (v & ~1) * 8 * magstep);
						XCopyArea(display, plane3, plane3, planegc,
						          z * 8 * magstep, (v & ~1) * 8 * magstep,
						          16 * magstep, 16 * magstep,
						          (x & ~1) * 8 * magstep, (v & ~1) * 8 * magstep);
						XCopyArea(display, bgplane, bgplane, planegc,
						          z * 8 * magstep, (v & ~1) * 8 * magstep,
						          16 * magstep, 16 * magstep,
						          (x & ~1) * 8 * magstep, (v & ~1) * 8 * magstep);
						bgmask_changed[currentcache] = 1;
						bitplanecachevalid[currentcache][(v & ~1) * 64 + (x & ~1)] =
						bitplanecachevalid[currentcache][(v & ~1) * 64 + (x | 1)] =
						bitplanecachevalid[currentcache][(v | 1) * 64 + (x & ~1)] =
						bitplanecachevalid[currentcache][(v | 1) * 64 + (x | 1)] = 1;
					}
					displaycachevalid[currentcache][(v & ~1) * 64 + (x & ~1)] =
					displaycachevalid[currentcache][(v & ~1) * 64 + (x | 1)] =
					displaycachevalid[currentcache][(v | 1) * 64 + (x & ~1)] =
					displaycachevalid[currentcache][(v | 1) * 64 + (x | 1)] = 1;
				} else {
					/* Assemble a group of 4 tiles from the respective bitplanes */
					count++;      /* For profiling/debugging */

					/* upper left tile */
					XCopyArea(display, tilecache1[currentcache], plane1,
					          planegc, (vramgroupptr[0] & 0xF8) * magstep,
					          ((vramgroupptr[0] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, x * 8 * magstep, v * 8 * magstep);
					XCopyArea(display, tilecache2[currentcache], plane2,
					          planegc, (vramgroupptr[0] & 0xF8) * magstep,
					          ((vramgroupptr[0] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, x * 8 * magstep, v * 8 * magstep);
					XCopyArea(display, tilecache3[currentcache], plane3,
					          planegc, (vramgroupptr[0] & 0xF8) * magstep,
					          ((vramgroupptr[0] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, x * 8 * magstep, v * 8 * magstep);
					XCopyArea(display, tilebgmask[currentcache], bgplane,
					          planegc, (vramgroupptr[0] & 0xF8) * magstep,
					          ((vramgroupptr[0] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, x * 8 * magstep, v * 8 * magstep);

					/* upper right tile */
					XCopyArea(display, tilecache1[currentcache], plane1,
					          planegc, (vramgroupptr[1] & 0xF8) * magstep,
					          ((vramgroupptr[1] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, (x + 1) * 8 * magstep, v * 8 * magstep);
					XCopyArea(display, tilecache2[currentcache], plane2,
					          planegc, (vramgroupptr[1] & 0xF8) * magstep,
					          ((vramgroupptr[1] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, (x + 1) * 8 * magstep, v * 8 * magstep);
					XCopyArea(display, tilecache3[currentcache], plane3,
					          planegc, (vramgroupptr[1] & 0xF8) * magstep,
					          ((vramgroupptr[1] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, (x + 1) * 8 * magstep, v * 8 * magstep);
					XCopyArea(display, tilebgmask[currentcache], bgplane,
					          planegc, (vramgroupptr[1] & 0xF8) * magstep,
					          ((vramgroupptr[1] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, (x + 1) * 8 * magstep, v * 8 * magstep);

					/* lower left tile */
					XCopyArea(display, tilecache1[currentcache], plane1,
					          planegc, (vramgroupptr[32] & 0xF8) * magstep,
					          ((vramgroupptr[32] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, x * 8 * magstep, (v + 1) * 8 * magstep);
					XCopyArea(display, tilecache2[currentcache], plane2,
					          planegc, (vramgroupptr[32] & 0xF8) * magstep,
					          ((vramgroupptr[32] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, x * 8 * magstep, (v + 1) * 8 * magstep);
					XCopyArea(display, tilecache3[currentcache], plane3,
					          planegc, (vramgroupptr[32] & 0xF8) * magstep,
					          ((vramgroupptr[32] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, x * 8 * magstep, (v + 1) * 8 * magstep);
					XCopyArea(display, tilebgmask[currentcache], bgplane,
					          planegc, (vramgroupptr[32] & 0xF8) * magstep,
					          ((vramgroupptr[32] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, x * 8 * magstep, (v + 1) * 8 * magstep);

					/* lower right tile */
					XCopyArea(display, tilecache1[currentcache], plane1,
					          planegc, (vramgroupptr[33] & 0xF8) * magstep,
					          ((vramgroupptr[33] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, (x + 1) * 8 * magstep, (v + 1) * 8 * magstep);
					XCopyArea(display, tilecache2[currentcache], plane2,
					          planegc, (vramgroupptr[33] & 0xF8) * magstep,
					          ((vramgroupptr[33] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, (x + 1) * 8 * magstep, (v + 1) * 8 * magstep);
					XCopyArea(display, tilecache3[currentcache], plane3,
					          planegc, (vramgroupptr[33] & 0xF8) * magstep,
					          ((vramgroupptr[33] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, (x + 1) * 8 * magstep, (v + 1) * 8 * magstep);
					XCopyArea(display, tilebgmask[currentcache], bgplane,
					          planegc, (vramgroupptr[33] & 0xF8) * magstep,
					          ((vramgroupptr[33] & 7) << 3) * magstep,
					          8 * magstep, 8 * magstep, (x + 1) * 8 * magstep, (v + 1) * 8 * magstep);

					bgmask_changed[currentcache] = 1;
					if (indexedcolor) {
						color1 = colortableX11[tilecolor * 3];
						color2 = colortableX11[tilecolor * 3 + 1];
						color3 = colortableX11[tilecolor * 3 + 2];
					} else {
						color1 = paletteX11[VRAM[0x3f01 + (tilecolor << 2)] & 63];
						color2 = paletteX11[VRAM[0x3f02 + (tilecolor << 2)] & 63];
						color3 = paletteX11[VRAM[0x3f03 + (tilecolor << 2)] & 63];
					}
					XFillRectangle(display, background[currentcache],
					               blackgc, x * 8 * magstep, v * 8 * magstep,
					               16 * magstep, 16 * magstep);

					if (color1) {
						XSetBackground(display, color1gc, black);
						XSetForeground(display, color1gc, color1);
						XCopyPlane(display, plane1, background[currentcache], color1gc,
						           x * 8 * magstep, v * 8 * magstep,
						           16 * magstep, 16 * magstep,
						           x * 8 * magstep, v * 8 * magstep, 1);
					}
					if (color2) {
						XSetBackground(display, color2gc, black);
						XSetForeground(display, color2gc, color2);
						XCopyPlane(display, plane2, background[currentcache], color2gc,
						           x * 8 * magstep, v * 8 * magstep,
						           16 * magstep, 16 * magstep,
						           x * 8 * magstep, v * 8 * magstep, 1);
					}
					if (color3) {
						XSetBackground(display, color3gc, black);
						XSetForeground(display, color3gc, color3);
						XCopyPlane(display, plane3, background[currentcache], color3gc,
						           x * 8 * magstep, v * 8 * magstep,
						           16 * magstep, 16 * magstep,
						           x * 8 * magstep, v * 8 * magstep, 1);
					}
					XCopyPlane(display, bgplane, background[currentcache], bgcolorgc,
					           x * 8 * magstep, v * 8 * magstep,
					           16 * magstep, 16 * magstep,
					           x * 8 * magstep, v * 8 * magstep, 1);
					if ((scanlines != 100) && (magstep > 1)) {
						for (int y = 0; y < 16; y++) {
							XFillRectangle(display, background[currentcache],
							               blackgc,
							               x * 8 * magstep,
							               (v * 8 + y) * magstep + 1,
							               (16 + 1) * magstep - 1,
							               magstep - 1);
						}
					}
					bitplanecachevalid[currentcache][(v & ~1) * 64 + (x & ~1)] =
					bitplanecachevalid[currentcache][(v & ~1) * 64 + (x | 1)] =
					bitplanecachevalid[currentcache][(v | 1) * 64 + (x & ~1)] =
					bitplanecachevalid[currentcache][(v | 1) * 64 + (x | 1)] = 1;
					displaycachevalid[currentcache][(v & ~1) * 64 + (x & ~1)] =
					displaycachevalid[currentcache][(v & ~1) * 64 + (x | 1)] =
					displaycachevalid[currentcache][(v | 1) * 64 + (x & ~1)] =
					displaycachevalid[currentcache][(v | 1) * 64 + (x | 1)] = 1;
				}
				cachegroupptr[0] = vramgroupptr[0];
				cachegroupptr[1] = vramgroupptr[1];
				cachegroupptr[64] = vramgroupptr[32];
				cachegroupptr[65] = vramgroupptr[33];
				redrawbackground = 1;
				redrawall = 1;
				displaytilecache[currentcache][(v & ~1) * 32 + (x >> 1)] =
				  ((cachegroupptr[0] << 24) |
				   (cachegroupptr[1] << 16) |
				   (cachegroupptr[64] << 8) |
				   (cachegroupptr[65]));
				displaycolorcache[currentcache][(v & ~1) * 32 + (x >> 1)] = tilecolor;
			}
		}
	}
	/* for debugging */
	/*if (count)printf("bg tiles changed: %d\n", count); */
}

static void
UpdateTilesOldX11(void)
{
	int x, y, l, n;
	int b1, b2;
/*	int loc, invloc, h, v; */
	int h, v;
	int vfirstchange = 64;
	int hfirstchange = 256;
	int vlastchange = 0;
	int hlastchange = 0;
	/*unsigned char transparent, opaque; */
	unsigned char *vramptr;
	int baseaddr = ((linereg[currentline] & 0x10) << 8) ^ (debug_switchtable << 12);        /* 0 or 0x1000 */

	static int count = 0;
	/* This can be used to skip frames for testing purposes */
	/*count++; if (count<100) return; */
	count = 0;

	/* Some games change the bitmaps on every frame, eg to display rotating
	   blocks.  In this case it's best to cache all the different bitmaps,
	   hence the use of multiple caches.  The size of the cache is defined
	   by tilecachedepth. */

	for (x = 0; x < 256; x++) {
		if ((((long long *)(VRAM + baseaddr))[x * 2] != ((long long *)(vramcache[currentcache]))[x * 2])
		 || (((long long *)(VRAM + 8 + baseaddr))[x * 2] != ((long long *)(vramcache[currentcache] + 8))[x * 2]))
			break;
	}
	if (x < 256) {
		for (y = (currentcache + 1) % tilecachedepth;
		     y != (currentcache + tilecachedepth - 1) % tilecachedepth;
		     y = (y + 1) % tilecachedepth) {
			for (x = 0;
			     x < 256
			  && ((((long long *)(VRAM + baseaddr))[x * 2] == ((long long *)(vramcache[y]))[x * 2])
			   && (((long long *)(VRAM + 8 + baseaddr))[x * 2] == ((long long *)(vramcache[y] + 8))[x * 2]));
			     x++);
			if (x == 256) {
				currentcache = y;
				break;
			}
		}
		if (x < 256) {
			/* When the cache is full, a cache entry must be overwritten to
			   make room for a new one.  Copy the current image to the new
			   cache entry. */
			if (nextcache == currentcache)
				nextcache = (nextcache + 1) % tilecachedepth;
			/* FIXME: only dirty a rectangle */
			for (x = 0; x < 256; x++)
				tiledirty[nextcache][x] = tiledirty[currentcache][x];
			bgcolor[nextcache] = bgcolor[currentcache];
			bgmask_changed[nextcache] =
			tilebgmask_changed[nextcache] =
			tilemask1_changed[nextcache] =
			tilemask2_changed[nextcache] =
			tilemask3_changed[nextcache] = 1;
			memcpy(ntcache[nextcache], ntcache[currentcache],
			       sizeof ntcache[nextcache]);
			memcpy(vramcache[nextcache], vramcache[currentcache],
			       sizeof vramcache[nextcache]);
			memset(displaycachevalid[currentcache], 0,
			       sizeof displaycachevalid[currentcache]);
			memset(bitplanecachevalid[currentcache], 0,
			       sizeof bitplanecachevalid[currentcache]);
			memcpy(displaytilecache[nextcache], displaytilecache[currentcache],
			       sizeof displaytilecache[nextcache]);
			memcpy(displaycolorcache[nextcache], displaycolorcache[currentcache],
			       sizeof displaycolorcache[nextcache]);
			/*memcpy(tiletransparent[nextcache], tiletransparent[currentcache], 512); */
			/*memcpy(tileopaque[nextcache], tileopaque[currentcache], 512); */
			XCopyArea(display, background[currentcache],
			          background[nextcache],
			          gc, 0, 0, 512 * magstep, 480 * magstep, 0, 0);
			XCopyArea(display, bgplane1[currentcache], bgplane1[nextcache],
			          planegc, 0, 0, 512 * magstep, 480 * magstep, 0, 0);
			XCopyArea(display, bgplane2[currentcache], bgplane2[nextcache],
			          planegc, 0, 0, 512 * magstep, 480 * magstep, 0, 0);
			XCopyArea(display, bgplane3[currentcache], bgplane3[nextcache],
			          planegc, 0, 0, 512 * magstep, 480 * magstep, 0, 0);
			XCopyArea(display, bgmask[currentcache], bgmask[nextcache],
			          planegc, 0, 0, 512 * magstep, 480 * magstep, 0, 0);
			XCopyArea(display, tilecache1[currentcache],
			          tilecache1[nextcache],
			          planegc, 0, 0, 256 * magstep, 64 * magstep, 0, 0);
			XCopyArea(display, tilecache2[currentcache],
			          tilecache2[nextcache],
			          planegc, 0, 0, 256 * magstep, 64 * magstep, 0, 0);
			XCopyArea(display, tilecache3[currentcache],
			          tilecache3[nextcache],
			          planegc, 0, 0, 256 * magstep, 64 * magstep, 0, 0);
			XCopyArea(display, tilebgmask[currentcache],
			          tilebgmask[nextcache],
			          planegc, 0, 0, 256 * magstep, 64 * magstep, 0, 0);
			/* for debugging */
			/*printf("invalidate cache %d (current=%d)\n", nextcache, currentcache); */
			currentcache = nextcache;
			nextcache = (nextcache + 1) % tilecachedepth;
		}
		/* for debugging */
		/*printf("currentcache=%d %d\n", y, x); */
	}

	/* This mess just compares each 16 bytes of VRAM with what's in our cache.
	   If a tile has changed, set the 'dirty' flag so we will redraw the
	   graphics for that tile. */

	for (x = 0; x < 256; x++)
		if ((((long long *)(VRAM + baseaddr))[x * 2] != ((long long *)(vramcache[currentcache]))[x * 2])
		 || (((long long *)(VRAM + 8 + baseaddr))[x * 2] != ((long long *)(vramcache[currentcache] + 8))[x * 2])) {
			tiledirty[currentcache][x] = 1;
			((long long *)(vramcache[currentcache]))[x * 2] = ((long long *)(VRAM + baseaddr))[x * 2];
			((long long *)(vramcache[currentcache] + 8))[x * 2] = ((long long *)(VRAM + 8 + baseaddr))[x * 2];
		}

	for (x = 0; x < 256; x++) {
		if (tiledirty[currentcache][x]) {
			v = ((x & 7) << 3);
			h = (x & 0xf8);
			if (h < hfirstchange)
				hfirstchange = h;
			if (v < vfirstchange)
				vfirstchange = v;
			if (h > hlastchange)
				hlastchange = h;
			if (v > vlastchange)
				vlastchange = v;
			if (v < 0 || v >= 64
			 || h < 0 || h >= 256)
				exit(EXIT_FAILURE);  /* sanity check - this should not happen! */
		}
	}
	for (h = hfirstchange; h <= hlastchange; h += 8) {
		for (v = vfirstchange; v <= vlastchange; v += 8) {
			x = h | ((v >> 3) & 7);
			for (l = 0; l < 8; l++) {
				int c;

				b1 = VRAM[baseaddr + ((x & 256) << 4) + ((x & 255) * 16) + l];
				b2 = VRAM[baseaddr + ((x & 256) << 4) + ((x & 255) * 16) + l + 8];
				for (n = 0, c = 0x80; n < 8; n++, c >>= 1)
					for (int yo = 0;
					     yo < ((scanlines == 100) ? magstep : 1);
					     yo++)
						for (int xo = 0; xo < magstep; xo++) {
							XPutPixel(tilebitimage,
							          (h + n) * magstep + xo,
							          (v + l) * magstep + yo,
							          (b1 & c) ? 1 : 0);
							XPutPixel(tilebitimage2,
							          (h + n) * magstep + xo,
							          (v + l) * magstep + yo,
							          (b2 & c) ? 1 : 0);
						}
			}
			tiledirty[currentcache][x] = 0;
			tilechanged[x] = redrawbackground = 1;
		}
	}

	if (hfirstchange <= hlastchange) {
		tilebgmask_changed[currentcache] =
		tilemask1_changed[currentcache] =
		tilemask2_changed[currentcache] =
		tilemask3_changed[currentcache] = 1;
		/* debug */
		/*printf("UpdateTilesOldX11: h=%d-%d (%d) v=%d-%d (%d),\n", hfirstchange, hlastchange, hlastchange-hfirstchange+8, vfirstchange, vlastchange, vlastchange-vfirstchange+8); */
		XPutImage(display, tilecache1[currentcache], planegc, tilebitimage,
		          hfirstchange * magstep, vfirstchange * magstep,
		          hfirstchange * magstep, vfirstchange * magstep,
		          (hlastchange - hfirstchange + 8) * magstep,
		          (vlastchange - vfirstchange + 8) * magstep);
		XPutImage(display, tilecache2[currentcache], planegc, tilebitimage2,
		          hfirstchange * magstep, vfirstchange * magstep,
		          hfirstchange * magstep, vfirstchange * magstep,
		          (hlastchange - hfirstchange + 8) * magstep,
		          (vlastchange - vfirstchange + 8) * magstep);

		/* bitwise-nor to create the background mask */
		XCopyArea(display, tilecache1[currentcache],
		          tilebgmask[currentcache], planegc,
		          hfirstchange * magstep, vfirstchange * magstep,
		          (hlastchange - hfirstchange + 8) * magstep,
		          (vlastchange - vfirstchange + 8) * magstep,
		          hfirstchange * magstep, vfirstchange * magstep);
		XCopyArea(display, tilecache2[currentcache],
		          tilebgmask[currentcache], planenorgc,
		          hfirstchange * magstep, vfirstchange * magstep,
		          (hlastchange - hfirstchange + 8) * magstep,
		          (vlastchange - vfirstchange + 8) * magstep,
		          hfirstchange * magstep, vfirstchange * magstep);
		/* Seperate the colors */
		XCopyArea(display, tilecache1[currentcache],
		          tilecache3[currentcache], planegc,
		          hfirstchange * magstep, vfirstchange * magstep,
		          (hlastchange - hfirstchange + 8) * magstep,
		          (vlastchange - vfirstchange + 8) * magstep,
		          hfirstchange * magstep, vfirstchange * magstep);
		XCopyArea(display, tilecache2[currentcache],
		          tilecache3[currentcache], planeandgc,
		          hfirstchange * magstep, vfirstchange * magstep,
		          (hlastchange - hfirstchange + 8) * magstep,
		          (vlastchange - vfirstchange + 8) * magstep,
		          hfirstchange * magstep, vfirstchange * magstep);
		XCopyArea(display, tilecache3[currentcache],
		          tilecache1[currentcache], planexorgc,
		          hfirstchange * magstep, vfirstchange * magstep,
		          (hlastchange - hfirstchange + 8) * magstep,
		          (vlastchange - vfirstchange + 8) * magstep,
		          hfirstchange * magstep, vfirstchange * magstep);
		XCopyArea(display, tilecache3[currentcache],
		          tilecache2[currentcache], planexorgc,
		          hfirstchange * magstep, vfirstchange * magstep,
		          (hlastchange - hfirstchange + 8) * magstep,
		          (vlastchange - vfirstchange + 8) * magstep,
		          hfirstchange * magstep, vfirstchange * magstep);
		/* Invalidate the cache */
		for (y = 0; y < 60; y++) {
			for (x = 0; x < 63; x++) {
				if (hvmirror == 0) {
					vramptr = VRAM + 0x2000 + ((x & 32) << 5) + (y % 30) * 32
					  + 0x400 * (y >= 30) + (x & 31);
				} else {
					vramptr = VRAM + ((0x2000 + (y % 30) * 32 + (x & 31)) ^
					                  (0x400 * (y >= 30)));
				}
				if (tilechanged[*vramptr]) {
					displaycachevalid[currentcache][y * 64 + x] = 0;
					bitplanecachevalid[currentcache][y * 64 + x] = 0;
					tilechanged[*vramptr] = 0;
				}
			}
		}
		/*desync=1; */
	}
}

static void
LayoutBackgroundOldX11(void)
{
	int z;
	unsigned int last;
	static int linecache[240];

	if ((!screen_on) || debug_bgoff) {
		if (redrawbackground == 0)
			return;
		XFillRectangle(display, layout,
		               solidbggc,
		               0, 0,
		               256 * magstep, 240 * magstep);
		if ((scanlines != 100) && (magstep > 1)) {
			for (int y = 0; y < 240; y++) {
				XFillRectangle(display, layout,
				               blackgc,
				               0, y * magstep + 1,
				               256 * magstep, magstep - 1);
			}
		}
		needsredraw = 1;
		return;
	} else {
		currentline = 0;
		UpdateTilesOldX11();
		if (!indexedcolor)
			UpdateTileColorsOldX11();
		DoBackgroundOldX11();
		if (redrawbackground == 0)
			return;
	}

	last = linereg[0] & 0x10;
	for (int y = 0; y < 240; y++) {
		z = y + 1;
		while (hscroll[y] == hscroll[z]
		    && vscroll[y] == vscroll[z]
		    && z < 240
		    && (linereg[y] & 0x10) == (linereg[z] & 0x10)
		    && (redrawall || scanline_diff[y] == scanline_diff[z]))
			z++;

		if (y && (linereg[y] & 0x10) != last) {
			currentline = y;
			last = linereg[y] & 0x10;
			UpdateTilesOldX11();
			if (!indexedcolor)
				UpdateTileColorsOldX11();
			DoBackgroundOldX11();
		}

		for (int x = y; x < z; x++)
			if (linecache[y] != currentcache)
				redrawall = 1;

		if (scanline_diff[y] || redrawall) {
			for (int x = y; x < z; x++)
				linecache[y] = currentcache;

			if (!osmirror) {
				if (vscroll[y] + z <= 480 || vscroll[y] + y >= 480)
					if (hscroll[y] <= 256) {
						XCopyArea(display, background[currentcache], layout, gc,
						          hscroll[y] * magstep, ((vscroll[y] + y) % 480) * magstep,
						          256 * magstep, (z - y) * magstep,
						          0, y * magstep);
					} else {
						XCopyArea(display, background[currentcache], layout, gc,
						          hscroll[y] * magstep, ((vscroll[y] + y) % 480) * magstep,
						          (512 - hscroll[y]) * magstep, (z - y) * magstep,
						          0, y * magstep);
						XCopyArea(display, background[currentcache], layout, gc,
						          0, ((vscroll[y] + y) % 480) * magstep,
						          (hscroll[y] - 256) * magstep, (z - y) * magstep,
						          (512 - hscroll[y]) * magstep, y * magstep);
					}
				else if (hscroll[y] <= 256) {
					XCopyArea(display, background[currentcache], layout, gc,
					          hscroll[y] * magstep, (vscroll[y] + y) * magstep,
					          256 * magstep, (480 - vscroll[y] - y) * magstep,
					          0, y * magstep);
					XCopyArea(display, background[currentcache], layout, gc,
					          hscroll[y] * magstep, 0,
					          256 * magstep, (vscroll[y] + z - 480) * magstep,
					          0, (480 - vscroll[y]) * magstep);
				} else {
					XCopyArea(display, background[currentcache], layout, gc,
					          hscroll[y] * magstep, (vscroll[y] + y) * magstep,
					          (512 - hscroll[y]) * magstep, (480 - vscroll[y] - y) * magstep,
					          0, y * magstep);
					XCopyArea(display, background[currentcache], layout, gc,
					          hscroll[y] * magstep, 0,
					          (512 - hscroll[y]) * magstep, (vscroll[y] + z - 480) * magstep,
					          0, (480 - vscroll[y]) * magstep);
					XCopyArea(display, background[currentcache], layout, gc,
					          0, (vscroll[y] + y) * magstep,
					          (hscroll[y] - 256) * magstep, (480 - vscroll[y] - y) * magstep,
					          (512 - hscroll[y]) * magstep, y * magstep);
					XCopyArea(display, background[currentcache], layout, gc,
					          0, 0,
					          (hscroll[y] - 256) * magstep, (vscroll[y] + z - 480) * magstep,
					          (512 - hscroll[y]) * magstep, (480 - vscroll[y]) * magstep);
				}
			} else /*if(osmirror) */ {
				if ((vscroll[y] + z - 1) % 240 >= (vscroll[y] + y) % 240)
					XCopyArea(display, background[currentcache], layout, gc,
					          (hscroll[y] & 255) * magstep, ((vscroll[y] + y) % 240) * magstep,
					          (256 - (hscroll[y] & 255)) * magstep, (z - y) * magstep,
					          0, (y % 240) * magstep);
				else {
					XCopyArea(display, background[currentcache], layout, gc,
					          (hscroll[y] & 255) * magstep, ((vscroll[y] + y) % 240) * magstep,
					          (256 - (hscroll[y] & 255)) * magstep, ((480 - vscroll[y] - y) % 240) * magstep,
					          0, (y % 240) * magstep);
					XCopyArea(display, background[currentcache], layout, gc,
					          (hscroll[y] & 255) * magstep, 0,
					          (256 - (hscroll[y] & 255)) * magstep, ((vscroll[y] + z) % 240) * magstep,
					          0, ((480 - vscroll[y]) % 240) * magstep);
				}

				if (hscroll[y] & 255) {
					if ((vscroll[y] + z - 1) % 240 >= (vscroll[y] + y) % 240)
						XCopyArea(display, background[currentcache], layout, gc,
						          0, ((vscroll[y] + y) % 240) * magstep,
						          (hscroll[y] & 255) * magstep, (z - y) * magstep,
						          ((512 - hscroll[y]) & 255) * magstep, (y % 240) * magstep);
					else {
						XCopyArea(display, background[currentcache], layout, gc,
						          0, ((vscroll[y] + y) % 240) * magstep,
						          (hscroll[y] & 255) * magstep, ((480 - vscroll[y] - y) % 240) * magstep,
						          ((512 - hscroll[y]) & 255) * magstep, (y % 240) * magstep);
						XCopyArea(display, background[currentcache], layout, gc,
						          0, 0,
						          (hscroll[y] & 255) * magstep, ((vscroll[y] + z) % 240) * magstep,
						          ((512 - hscroll[y]) & 255) * magstep, ((480 - vscroll[y]) % 240) * magstep);
					}
				}
			}
		}
		y = z - 1;
	}
	needsredraw = 1;
}

/* Update the colors on the screen if the palette changed */
void
UpdateColorsX11(void)
{
	/* Set Background color */
	oldbgcolor = currentbgcolor;
	if (indexedcolor) {
		color.pixel = currentbgcolor = colortableX11[24];
		color.red = ((NES_palette[VRAM[0x3f00] & 63] & 0xFF0000) >> 8);
		color.green = (NES_palette[VRAM[0x3f00] & 63] & 0xFF00);
		color.blue = ((NES_palette[VRAM[0x3f00] & 63] & 0xFF) << 8);
		color.flags = DoRed | DoGreen | DoBlue;
		XStoreColor(display, colormap, &color);
		XSetForeground(display, solidbggc, currentbgcolor);
		XSetForeground(display, bgcolorgc, currentbgcolor);
		XSetForeground(display, backgroundgc, currentbgcolor);
	} else /* truecolor */ {
		currentbgcolor = paletteX11[VRAM[0x3f00] & 63];
		if ((renderer->_flags & RENDERER_DIFF) && ((bpp != 8) || (magstep > 1)))
			palette[24] = VRAM[0x3f00] & 63;
		else
			palette[24] = currentbgcolor;
		if (scanlines && (scanlines != 100))
			palette2[24] = palette2X11[VRAM[0x3f00] & 63];
		if (oldbgcolor != currentbgcolor /*||currentbgcolor!=bgcolor[currentcache] */) {
			XSetForeground(display, solidbggc, currentbgcolor);
			XSetForeground(display, bgcolorgc, currentbgcolor);
			XSetForeground(display, backgroundgc, currentbgcolor);
			redrawbackground = 1;
			needsredraw = 1;
		}
	}

	/* Tile colors */
	if (indexedcolor) {
		for (int x = 0; x < 24; x++) {
			if (VRAM[0x3f01 + x + (x / 3)] != palette_cache[0][1 + x + (x / 3)]) {
				color.pixel = colortableX11[x];
				color.red = ((NES_palette[VRAM[0x3f01 + x + (x / 3)] & 63] & 0xFF0000) >> 8);
				color.green = (NES_palette[VRAM[0x3f01 + x + (x / 3)] & 63] & 0xFF00);
				color.blue = ((NES_palette[VRAM[0x3f01 + x + (x / 3)] & 63] & 0xFF) << 8);
				color.flags = DoRed | DoGreen | DoBlue;
				XStoreColor (display, colormap, &color);
				/*printf("color %d (%d) = %6x\n", x, colortableX11[x], paletteX11[VRAM[0x3f01+x+(x/3)]&63]); */
			}
		}
		memcpy(palette_cache[0], VRAM + 0x3f00, 32);
	}

	/* Set palette tables */
	if (indexedcolor) {
		/* Already done in InitDisplayX11 */
	} else /* truecolor */ {
		for (int x = 0; x < 24; x++) {
			if ((renderer->_flags & RENDERER_DIFF) && ((bpp != 8) || (magstep > 1)))
				palette[x] = VRAM[0x3f01 + x + (x / 3)] & 63;
			else
				palette[x] = paletteX11[VRAM[0x3f01 + x + (x / 3)] & 63];
			if (scanlines && (scanlines != 100))
				palette2[x] = palette2X11[VRAM[0x3f01 + x + (x / 3)] & 63];
		}
	}
}

/* Update the tile colors for the current cache if the palette changed */
/* (direct color mode only) */
static void
UpdateTileColorsOldX11(void)
{
	/* Set Background color */

	if (currentbgcolor != bgcolor[currentcache]) {
		bgcolor[currentcache] = currentbgcolor;
		redrawbackground = 1;
		needsredraw = 1;
		if (bgmask_changed[currentcache] || currentcache != current_bgmask) {
			XSetClipMask(display, backgroundgc, bgmask[currentcache]);
			bgmask_changed[current_bgmask = currentcache] = 0;
		}
		XFillRectangle(display, background[currentcache],
		               backgroundgc,
		               0, 0,
		               512 * magstep, 480 * magstep);
		if ((scanlines != 100) && (magstep > 1)) {
			for (int y = 0; y < 480; y++) {
				XFillRectangle(display, background[currentcache],
				               blackgc,
				               0, y * magstep + 1,
				               512 * magstep, magstep - 1);
			}
		}
		redrawall = 1;
	}

	/* Tile colors */

	/* For programs which change the palettes affecting only a few tiles,
	   in truecolor mode, it's best to just invalidate the cache and redraw
	   those few tiles, as done below.  For programs which change the colors
	   across the whole screen, it's best to paint the whole thing using a
	   clipmask, as for the background above.  Will have to see what works
	   best.  This only affects truecolor mode; for 8-bit mode, just change
	   the palettes. */

	for (int y = 0; y < 30; y++) {
		for (int x = 0; x < 32; x++) {
			int t = displaycolorcache[currentcache][y * 64 + x];
			if (palette_cache[currentcache][t * 4 + 1] != VRAM[0x3f00 + t * 4 + 1]
			 || palette_cache[currentcache][t * 4 + 2] != VRAM[0x3f00 + t * 4 + 2]
			 || palette_cache[currentcache][t * 4 + 3] != VRAM[0x3f00 + t * 4 + 3]) {
				displaycachevalid[currentcache][y * 128 + x * 2] = 0;
			}
		}
	}
	memcpy(palette_cache[currentcache], VRAM + 0x3f00, 32);

	/* Sprite colors for truecolor mode are handled by DrawSpritesOldX11() */
}


/* This looks up a pixel on the screen and returns 1 if it is in the
   foreground and 0 if it is in the background.  This is used for the
   sprite transparency. */
static int
InForegroundOldX11(unsigned int x, unsigned int y)
{
	unsigned int          h, v;
	unsigned int          tile;
	unsigned int          baseaddr = ((linereg[y] & 0x10) << 8) ^ (debug_switchtable << 12);       /* 0 or 0x1000 */
	unsigned char         d1, d2;
	unsigned int          page;

	x += hscroll[y];
	y += vscroll[y];
	h = ((x & 255) >> 3);
	v = ((y % 240) >> 3);
	if (nomirror)
		page = 0x2000 + ((x & 256) << 2) + (((y % 480) >= 240) << 10);
	else if (hvmirror)
		page = 0x2000 + (((y % 480) >= 240) << 10);
	else
		page = 0x2000 + ((x & 256) << 2);
	tile = VRAM[page + h + (v << 5)];
	d1 = VRAM[baseaddr + (tile << 4) + (y & 7)];
	d2 = VRAM[baseaddr + (tile << 4) + (y & 7) + 8];
	return ((d1 | d2) >> (~x & 7)) & 1;
}

static void
DrawSpritesOldX11(void)
{
/*	int spritetile, spritecolor, pixelcolor, pixelcolor2; */
	int spritetile;
	int hflip, vflip, behind;
	int d1, d2;
/*	int v, n, b1, b2, b3, b4; */
	int baseaddr = (RAM[0x2000] & 0x08) << 9;     /* 0x0 or 0x1000 */
	int spritesize = 8 << ((RAM[0x2000] & 0x20) >> 5);    /* 8 or 16 */
/*	unsigned int color1, color2, color3; */
	unsigned int color1;
/*	static unsigned int currentcolor1, currentcolor2, currentcolor3, currentsolidcolor; */
	static unsigned int currentcolor1;
/*	static int currentmask1=-1, currentmask2=-1, currentmask3=-1, currentmaskbg=-1; */
/*	static unsigned int random; */
	static unsigned long long sprite_palette_cache[2];
	signed char linebuffer[256];

	if (debug_spritesoff)
		return;

	/* If the sprite palette changed, we must redraw the sprites in
	   static color mode.  This isn't necessary for indexed color mode */
	if (!indexedcolor
	 && (*((long long *)(VRAM + 0x3f10)) != sprite_palette_cache[0]
	  || *((long long *)(VRAM + 0x3f18)) != sprite_palette_cache[1])) {
		memcpy(sprite_palette_cache, VRAM + 0x3f10, 16);
		redrawall = needsredraw = 1;
	}

	/* If any sprite entries have changed since last time, redraw */
	for (int s = 0; s < 64; s++) {
		if (spriteram[s * 4] < 240 || spritecache[s * 4] < 240) {
			if (((int *)spriteram)[s] != ((int *)spritecache)[s]) {
				redrawall = needsredraw = 1;
				((int *)spritecache)[s] = ((int *)spriteram)[s];
			}
		}
	}

	if (redrawbackground || redrawall) {
		for (int y = 1; y < 240; y++) {
			if (redrawall || scanline_diff[y]) {
				memset(linebuffer, 0, 256);      /* Clear buffer for this scanline */
				baseaddr = ((linereg[y] & 0x08) << 9);    /* 0 or 0x1000 */
				for (int s = 63; s >= 0; s--) {
					if (spriteram[s * 4] < y
					 && spriteram[s * 4] < 240
					 && spriteram[s * 4 + 3] < 249) {
						if (spriteram[s * 4] + spritesize >= y) {
							spritetile = spriteram[s * 4 + 1];
							if (spritesize == 16)
								baseaddr = (spritetile & 1) << 12;
							behind = spriteram[s * 4 + 2] & 0x20;
							hflip = spriteram[s * 4 + 2] & 0x40;
							vflip = spriteram[s * 4 + 2] & 0x80;

							/*linebuffer[spriteram[s*4+3]]=1; */
							/*XDrawPoint(display, layout, spritecolor3gc, spriteram[s*4+3], y); */

							/* This finds the memory location of the tiles, taking into account
							   that vertically flipped sprites are in reverse order. */
							if (vflip) {
								if (spriteram[s << 2] >= y - 8) {
									/* 8x8 sprites and first half of 8x16 sprites */
									d1 = VRAM[baseaddr + ((spritetile & (~(spritesize >> 4))) << 4) + spritesize * 2 - 8 - y + spriteram[s * 4]];
									d2 = VRAM[baseaddr + ((spritetile & (~(spritesize >> 4))) << 4) + spritesize * 2 - y + spriteram[s * 4]];
								} else {
									/* Do second half of 8x16 sprites */
									d1 = VRAM[baseaddr + ((spritetile & (~(spritesize >> 4))) << 4) + spritesize * 2 - 16 - y + spriteram[s * 4]];
									d2 = VRAM[baseaddr + ((spritetile & (~(spritesize >> 4))) << 4) + spritesize * 2 - 8 - y + spriteram[s * 4]];
								}
							} else {
								if (spriteram[s << 2] >= y - 8) {
									/* 8x8 sprites and first half of 8x16 sprites */
									d1 = VRAM[baseaddr + ((spritetile & (~(spritesize >> 4))) << 4) + y - 1 - spriteram[s * 4]];
									d2 = VRAM[baseaddr + ((spritetile & (~(spritesize >> 4))) << 4) + y + 7 - spriteram[s * 4]];
								} else {
									/* Do second half of 8x16 sprites */
									d1 = VRAM[baseaddr + ((spritetile & (~(spritesize >> 4))) << 4) + y + 7 - spriteram[s * 4]];
									d2 = VRAM[baseaddr + ((spritetile & (~(spritesize >> 4))) << 4) + y + 15 - spriteram[s * 4]];
								}
							}
							for (int x = 7 * (!hflip); x < 8 && x >= 0; x += 1 - ((!hflip) << 1)) {
								if (d1 & d2 & 1)
									linebuffer[spriteram[s * 4 + 3] + x] = 3 + ((spriteram[s * 4 + 2] & 3) << 2);
								else if (d1 & 1)
									linebuffer[spriteram[s * 4 + 3] + x] = 1 + ((spriteram[s * 4 + 2] & 3) << 2);
								else if (d2 & 1)
									linebuffer[spriteram[s * 4 + 3] + x] = 2 + ((spriteram[s * 4 + 2] & 3) << 2);
								if (behind && (d1 | d2))
									if (InForegroundOldX11(spriteram[s * 4 + 3] + x, y))
										linebuffer[spriteram[s * 4 + 3] + x] = 0;     /* Sprite hidden behind background */
								d1 >>= 1;
								d2 >>= 1;
							}
						}
					}
				}

				for (int x = 0; x < 256; x++) {
					if (linebuffer[x]) {
						if (indexedcolor)
							color1 = colortableX11[(linebuffer[x] >> 2) * 3 + 11 + (linebuffer[x] & 3)];
						else
							color1 = paletteX11[VRAM[0x3f10 + (linebuffer[x] & 15)] & 63];
						if (color1 != currentcolor1)
							XSetForeground(display, spritecolor1gc,
							               currentcolor1 = color1);

						{
							for (int yo = 0;
							     yo < ((scanlines == 100) ? magstep : 1);
							     yo++)
								for (int xo = 0; xo < magstep; xo++)
									XDrawPoint(display, layout, spritecolor1gc,
									           x * magstep + xo, y * magstep + yo);
						}
					}
				}
			}
		}
	}
}

static void
DiffUpdateOldX11(void)
{
	static int old_screen_on;
	static int old_sprites_on;
	static unsigned char oldlinereg[240];
	int spritesize = 8 << ((RAM[0x2000] & 0x20) >> 5);    /* 8 or 16 */

	if (old_screen_on != screen_on)
		redrawall = redrawbackground = 1;
	old_screen_on = screen_on;

	if (screen_on) {
		if (old_sprites_on != sprites_on)
			redrawall = needsredraw = 1;
		old_sprites_on = sprites_on;
	}

	/* Update scanline redraw info */
	for (unsigned int x = 0; x < 60; x++) {
		tileline_begin[x] = 512;
		tileline_end[x] = 256;
	}
	for (unsigned int x = 0; x < 240; x++) {
		scanline_diff[x] = 0;
		if (oldhscroll[x] != hscroll[x])
			redrawbackground = scanline_diff[x] = 1;
		oldhscroll[x] = hscroll[x];
		if (oldvscroll[x] != vscroll[x])
			redrawbackground = scanline_diff[x] = 1;
		oldvscroll[x] = vscroll[x];
		if (osmirror) {
			tileline_begin[((vscroll[x] + x) % 240) >> 3] = 0;
			tileline_end[((vscroll[x] + x) % 240) >> 3] = 256;
		} else {
			if (tileline_begin[((vscroll[x] + x) % 480) >> 3] > hscroll[x])
				tileline_begin[((vscroll[x] + x) % 480) >> 3] = hscroll[x];
			if (tileline_end[((vscroll[x] + x) % 480) >> 3] < hscroll[x] + 256)
				tileline_end[((vscroll[x] + x) % 480) >> 3] = hscroll[x] + 256;
			if (tileline_begin[(((vscroll[x] + x) % 480) >> 3) & 62] > hscroll[x])
				tileline_begin[(((vscroll[x] + x) % 480) >> 3) & 62] = hscroll[x];
			if (tileline_end[(((vscroll[x] + x) % 480) >> 3) & 62] < hscroll[x] + 256)
				tileline_end[(((vscroll[x] + x) % 480) >> 3) & 62] = hscroll[x] + 256;
		}
		if (oldlinereg[x] != linereg[x])
			redrawbackground = scanline_diff[x] = 1;
		oldlinereg[x] = linereg[x];
	}

	/* See if any sprite entries have changed since last time and mark */
	/* those scanlines for redraw. */
	if (debug_spritesoff || !sprites_on)
		return;
	for (unsigned int s = 0; s < 64; s++) {
		if (spritecache[s * 4] < 240) {
			if (((int *)spriteram)[s] != ((int *)spritecache)[s]) {
				redrawbackground = 1;
				for (unsigned int x = 1; x <= spritesize && x + spritecache[s * 4] < 240; x++)
					scanline_diff[x + spritecache[s * 4]] = 1;
			}
		}
	}
}

#endif /* HAVE_X */
