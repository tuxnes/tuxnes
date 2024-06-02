// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: This file handles the I/O subsystem for the X window
 * system.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <hqx.h>

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif /* HAVE_FEATRES_H */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_MACHINE_ENDIAN_H
#include <machine/endian.h>
#endif
#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif
#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif

#include "consts.h"
#include "controller.h"
#include "globals.h"
#include "joystick.h"
#include "mapper.h"
#include "renderer.h"
#include "screenshot.h"

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
#if defined(HAVE_LIBXPM) && defined(HAVE_X11_XPM_H)

#include <X11/xpm.h>

#define HAVE_XPM 1
#endif

/* check for X Extension support */
#if defined(HAVE_LIBXEXT) && defined(HAVE_X11_EXTENSIONS_XEXT_H)

#include <X11/extensions/Xext.h>

#define HAVE_XEXT 1
#endif

/* check for X Shared Memory extension */
#if defined(HAVE_XEXT) && defined(HAVE_SYS_IPC_H) && \
    defined(HAVE_SYS_SHM_H) && defined(HAVE_X11_EXTENSIONS_XSHM_H)
#define HAVE_SHM 1
#endif

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#ifndef X_ShmAttach
#include <X11/Xmd.h>
#include <X11/extensions/shmproto.h>
#endif
#endif

#ifdef HAVE_X11_EXTENSIONS_SCRNSAVER_H
#include <X11/extensions/scrnsaver.h>
#define HAVE_SCRNSAVER 1
#endif

#if defined(HAVE_LIBXRENDER) && defined(HAVE_X11_EXTENSIONS_XRENDER_H)
#define HAVE_XRENDER
#endif

#ifdef HAVE_XRENDER
#include <X11/extensions/Xrender.h>
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

/* X11 stuff: */
static Display  *display;
static int      (*oldhandler)(Display *, XErrorEvent *) = 0;
static Visual   *visual;
static Window    rootwindow, w;
static int       screen;
static Colormap colormap;

static unsigned int paletteX11[64];

static unsigned char    *keystate[32];
static GC       gc, blackgc;
static GC       backgroundgc, solidbggc;
static GC       bgcolorgc;
static int      black;
static XSizeHints       sizehints;
static XClassHint      classhints;
static XWMHints        wmhints;
static XGCValues        GCValues;
static unsigned long    colortableX11[25];
static XColor   color;

#ifdef HAVE_XRENDER
static Pixmap scalePixmap;
static Picture scalePicture, windowPicture;
#endif
static XImage   *image = 0;

// X11 virtual framebuffer
// This buffer will be passed into an XImage.
//
// If we are not applying any filters, it may be the same memory as fb.c's
// framebuffer, which represents the unaltered image of the console.
//
// Otherwise, it will be the output buffer of hqx.
//
static char     *xfb = 0;
static int      xfb_width, xfb_height;
static int      xfb_bytes_per_line = 0;

// This represents the window dimensions.
// Default is NES screen resolution, multiplied by scale factor.  However
// it may be resized by the WM or the user.
//
static int width, height;

// If set to a value other than 1, we will allocate a separate fb/rfb buffer
// for fb.c and invoke hqx with that factor.
//
// Note that magstep (which represents the XRender scale factor) can be set
// to a different value, eg. pehaps we HQX scale into xfb with one factor,
// then XRender scale into another.
//
int scaler_magstep = 1;

/* externel and forward declarations */
void    fbinit(void);
void    quit(void);
void    START(void);
static void     InitScreenshotX11(void);
static void     SaveScreenshotX11(void);
static void     HandleKeyboardX11(XEvent ev);


#ifdef HAVE_SHM

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
	status =
	  XpmWriteFileFromImage(display, screenshotfile, image, NULL, NULL);
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
#ifdef HAVE_SHM
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

static bool XRenderSupported()
{
#ifdef HAVE_XRENDER
	static bool checked = false, enabled = false;
	int eventBase, errorBase;
	if (!checked) {
		if (XRenderQueryExtension(display, &eventBase, &errorBase))
			enabled = true;
		checked = true;
	}
	return enabled;
#else
	return false;
#endif
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

	switch (scaler_magstep) {
	case 1:
	case 2:
	case 3:
	case 4:
		break;
	default:
		fprintf(stderr,
			"[%s] Scale factor %d is not valid for HQX\n",
			renderer->name, scaler_magstep);
		scaler_magstep = scaler_magstep > 1 ? 2 : 1;
	}
	if (magstep > maxsize) {
		fprintf(stderr,
		        "[%s] Enlargement factor %d is too large!\n",
		        renderer->name, magstep);
		magstep = maxsize;
	}
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
	if (scaler_magstep != 1 & depth != 32) {
		fprintf(stderr,
			"[%s] HQX will only work at higher bitdepths; disabling\n",
			renderer->name);
		scaler_magstep = 1;
	}
	if (scaler_magstep != 1) {
		hqxInit();
	}
	if (scaler_magstep > magstep) {
		magstep = scaler_magstep;
	}
	if ((BYTE_ORDER != BIG_ENDIAN) && (BYTE_ORDER != LITTLE_ENDIAN)
	 && (bpp == 32)) {
		fprintf(stderr,
		        "======================================================\n"
		        "Warning: %s-endian Host Detected\n"
		        "\n"
		        "This host may not handle 32bpp display properly.\n"
		        "======================================================\n",
		        (BYTE_ORDER == PDP_ENDIAN) ? "PDP" : "Obscure");
		fflush(stderr);
	}
	if ((visual->class & 1) && renderer_config.indexedcolor) {
		if (XAllocColorCells(display, colormap, 0, 0, 0, colortableX11, 25) == 0) {
			fprintf(stderr, "%s: [%s] Can't allocate colors!\n",
			        *argv, renderer->name);
			exit(EXIT_FAILURE);
		}
		/* Pre-initialize the colormap to known values */
		color.red   = NES_palette[0] >> 8 & 0xff00;
		color.green = NES_palette[0]      & 0xff00;
		color.blue  = NES_palette[0] << 8 & 0xff00;
		color.flags = DoRed | DoGreen | DoBlue;
		for (x = 0; x <= 24; x++) {
			color.pixel = colortableX11[x];
			palette[x] = color.pixel;
			XStoreColor(display, colormap, &color);
		}
	} else {
		renderer_config.indexedcolor = 0;
		/* convert palette to local color format */
		for (x = 0; x < 64; x++) {
			color.pixel = x;
			color.red   = NES_palette[x] >> 8 & 0xff00;
			color.green = NES_palette[x]      & 0xff00;
			color.blue  = NES_palette[x] << 8 & 0xff00;
			color.flags = DoRed | DoGreen | DoBlue;
			if (XAllocColor(display, colormap, &color) == 0) {
				fprintf(stderr, "%s: [%s] Can't allocate colors!\n",
				        *argv, renderer->name);
				exit(EXIT_FAILURE);
			}
			paletteX11[x] = color.pixel;
		}
	}
	xfb_width = 256 * scaler_magstep;
	xfb_height = 240 * scaler_magstep;
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
	XSetForeground(display, gc, ~0UL);
	XSetBackground(display, gc, 0UL);
	XSetForeground(display, blackgc, 0);
	XSetBackground(display, blackgc, 0);
	GCValues.function = GXor;
	bgcolorgc = XCreateGC(display, w, GCFunction, &GCValues);
	XSetBackground(display, bgcolorgc, black);
	GCValues.function = GXcopy;

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

	backgroundgc = XCreateGC(display, w, 0, 0);
	XSetBackground(display, backgroundgc, black);
	solidbggc = XCreateGC(display, w, 0, 0);
	XSetBackground(display, solidbggc, black);
	XMapWindow(display, w);
	XSelectInput(display, w, KeyPressMask | KeyReleaseMask | ExposureMask |
	             FocusChangeMask | KeymapStateMask | StructureNotifyMask);
	XFlush(display);
	gettimeofday(&time, NULL);
	renderer_data.basetime = time.tv_sec;
	if (magstep != scaler_magstep) {
		if (!XRenderSupported()) {
			fprintf(stderr, "x11: scaling requires XRENDER support\n");
			exit(1);
		}

#ifdef HAVE_XRENDER
		scalePixmap = XCreatePixmap(display, w, xfb_width, xfb_height, depth);

		XRenderPictFormat *fmt = XRenderFindVisualFormat(display, DefaultVisual(display, DefaultScreen(display)));
		XRenderPictureAttributes attrs = {0};

		windowPicture = XRenderCreatePicture(display, w, fmt, 0, &attrs);
		scalePicture = XRenderCreatePicture(display, scalePixmap, fmt, 0, &attrs);
#endif
	}
#ifdef HAVE_SHM
	if ((shm_status == True)) {
		if (depth == 1)
			shm_image = XShmCreateImage(display, visual, depth,
			                         XYBitmap, NULL, &shminfo,
			                         xfb_width, xfb_height);
		else
			shm_image = XShmCreateImage(display, visual, depth,
			                         ZPixmap, NULL, &shminfo,
			                         xfb_width, xfb_height);
		if (shm_image) {
			xfb_bytes_per_line = shm_image->bytes_per_line;
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
		        renderer_config.indexedcolor ? "dynamic" : "static",
		        image ? "shared-memory" : "plain",
		        "XImage");
	}
	if (!image) {
		xfb_bytes_per_line = xfb_width * bpp / 8;
		if (!(xfb = malloc(xfb_bytes_per_line * xfb_height))) {
			perror("malloc");
			exit(EXIT_FAILURE);
		}
		if (depth == 1)
			image = XCreateImage(display, visual, depth,
			                     XYBitmap, 0, xfb, xfb_width, xfb_height,
			                     8, bytes_per_line);
		else
			image = XCreateImage(display, visual, depth,
			                     ZPixmap, 0, xfb, xfb_width, xfb_height,
			                     bpp, bytes_per_line);
	}
	if (scaler_magstep != 1) {
		// Allocate unscaled image buffer
		bytes_per_line = 256 * bpp / 8;
		fb = rfb = malloc(bytes_per_line * 240 / 8);
		if (!fb) {
			perror("malloc");
			exit(EXIT_FAILURE);
		}
	} else {
		// No framebuffer scaling (though we might use XRender to upscale)
		// This means fb.c and the XImage share the same buffer.
		rfb = fb = xfb;
		bytes_per_line = xfb_bytes_per_line;
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
		case XK_F7:
		case XK_Print:
		case XK_S:
		case XK_s:
			SaveScreenshotX11();
			break;
		case XK_BackSpace:
			START();
			break;
		case XK_Pause:
		case XK_P:
		case XK_p:
			renderer_data.pause_display = !renderer_data.pause_display;
			break;
		case XK_grave:
			renderer_data.halfspeed = 1;
			renderer_data.doublespeed = 0;
			renderer_data.pause_display = 0;
			break;
		case XK_1:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 0;
			renderer_data.pause_display = 0;
			break;
		case XK_2:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 2;
			renderer_data.pause_display = 0;
			break;
		case XK_3:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 3;
			renderer_data.pause_display = 0;
			break;
		case XK_4:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 4;
			renderer_data.pause_display = 0;
			break;
		case XK_5:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 5;
			renderer_data.pause_display = 0;
			break;
		case XK_6:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 6;
			renderer_data.pause_display = 0;
			break;
		case XK_7:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 7;
			renderer_data.pause_display = 0;
			break;
		case XK_8:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 8;
			renderer_data.pause_display = 0;
			break;
		case XK_0:
			renderer_data.pause_display = 1;
			break;
		}
}

static void
RenderImage(XEvent *ev)
{
	Drawable target = w;
	int sx = 0, sy = 0;
	int w_ = xfb_width, h = xfb_height;
	int dx = (width - w_ * magstep) / 2;
	int dy = (height - h * magstep) / 2;

#ifdef HAVE_XRENDER
	int old_dx = dx;
	int old_dy = dy;

	if (magstep != scaler_magstep) {
		target = scalePixmap;
		dx = 0;
		dy = 0;
	}
#endif

	switch (scaler_magstep) {
	case 2:
		hq2x_32_rb((uint32_t*)fb, bytes_per_line, (uint32_t*)xfb, xfb_bytes_per_line, 256, 240);
		break;
	case 3:
		hq3x_32_rb((uint32_t*)fb, bytes_per_line, (uint32_t*)xfb, xfb_bytes_per_line, 256, 240);
		break;
	case 4:
		hq4x_32_rb((uint32_t*)fb, bytes_per_line, (uint32_t*)xfb, xfb_bytes_per_line, 256, 240);
		break;
	}

#ifdef HAVE_SHM
	if (shm_attached) {
		XShmPutImage(display, target, gc, image,
				sx, sy,
				dx, dy,
				w_, h,
				True);
		/* hang the event loop until we get a ShmCompletion */
		ev->type = -1;
	} else
#endif
	{
		XPutImage(display, target, gc, image,
				sx, sy,
				dx, dy,
				w_, h);
		XFlush(display);
	}

#ifdef HAVE_XRENDER
	if (magstep != scaler_magstep) {
		double xscale = (magstep + 0.0) / scaler_magstep, yscale = xscale;
		XTransform transform =
		{
			{
				{ XDoubleToFixed(1.0/xscale), XDoubleToFixed(0), XDoubleToFixed(0) },
				{ XDoubleToFixed(0), XDoubleToFixed(1.0/yscale), XDoubleToFixed(0) },
				{ XDoubleToFixed(0), XDoubleToFixed(0), XDoubleToFixed(1.0) }
			}
		};
		XRenderSetPictureTransform(display, scalePicture, &transform);
		XRenderComposite(display, PictOpSrc, scalePicture, None, windowPicture, 0,0, 0,0, old_dx, old_dy, w_*magstep, h*magstep);
	}
#endif
}

void
UpdateDisplayX11(void)
{
	static XEvent ev;
	struct timeval time;
	static unsigned int frame;
	unsigned int timeframe;
	static int nodisplay = 0;
#ifdef HAVE_SCRNSAVER
	static int sssuspend = 0;
#endif

	/* Check the time.  If we're getting behind, skip a frame to stay in sync. */
	gettimeofday(&time, NULL);
	timeframe = (time.tv_sec - renderer_data.basetime) * 50 + time.tv_usec / 20000;     /* PAL */
	timeframe = (time.tv_sec - renderer_data.basetime) * 60 + time.tv_usec / 16666;     /* NTSC */
	frame++;
	if (renderer_data.halfspeed)
		timeframe >>= 1;
	else if (renderer_data.doublespeed)
		timeframe *= renderer_data.doublespeed;
	if (renderer_data.desync) {
		renderer_data.desync = 0;
		frame = timeframe;
	} else if (frame < timeframe - 20 && frame % 20 == 0) {
		/* If we're more than 20 frames behind, might as well stop counting. */
		renderer_data.desync = 1;
	}

	if (!nodisplay) {
		drawimage(PBL);
		if (!frameskip) {
			UpdateColorsX11();
			RenderImage(&ev);
			renderer_data.redrawall = renderer_data.needsredraw = 0;
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
				renderer_data.pause_display = 0;
			if (ev.type == FocusOut)
				renderer_data.pause_display = 1;
#endif
			if (ev.type == MapNotify) {
				XExposeEvent *xev = (XExposeEvent *)&ev;

				xev->type = Expose;
				xev->count = 0;
				nodisplay = 0;
				renderer_data.needsredraw = renderer_data.redrawall = 1;
			} else if (ev.type == UnmapNotify) {
				nodisplay = 1;
				renderer_data.needsredraw = renderer_data.redrawall = 0;
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
					RenderImage(&ev);
				}
			}
			if (renderer_data.pause_display && renderer_data.needsredraw) {
				RenderImage(&ev);
				renderer_data.needsredraw = 0;
			}
		}

#ifdef HAVE_SCRNSAVER
		if (renderer_data.pause_display && sssuspend) {
			sssuspend = 0;
			XScreenSaverSuspend(display, 0);
		}
#endif

		if (renderer_data.pause_display) {
			usleep(16666);
			renderer_data.desync = 1;
		}
	} while (renderer_data.pause_display);

#ifdef HAVE_SCRNSAVER
	if (!sssuspend) {
		sssuspend = 1;
		XScreenSaverSuspend(display, 1);
	}
#endif

	renderer_data.needsredraw = 0;
	renderer_data.redrawbackground = 0;
	renderer_data.redrawall = 0;

	/* Check the time.  If we're getting behind, skip next frame to stay in sync. */
	gettimeofday(&time, NULL);
	timeframe = (time.tv_sec - renderer_data.basetime) * 60 + time.tv_usec / 16666;     /* NTSC */
	if (renderer_data.halfspeed)
		timeframe >>= 1;
	else if (renderer_data.doublespeed)
		timeframe *= renderer_data.doublespeed;
	if (frame >= timeframe || frame % 20 == 0)
		frameskip = 0;
	else
		frameskip = 1;
}

/* Update the colors on the screen if the palette changed */
void
UpdateColorsX11(void)
{
	/* Set Background color */
	oldbgcolor = currentbgcolor;
	if (renderer_config.indexedcolor) {
		color.pixel = currentbgcolor = colortableX11[24];
		color.red   = NES_palette[VRAM[0x3f00] & 63] >> 8 & 0xff00;
		color.green = NES_palette[VRAM[0x3f00] & 63]      & 0xff00;
		color.blue  = NES_palette[VRAM[0x3f00] & 63] << 8 & 0xff00;
		color.flags = DoRed | DoGreen | DoBlue;
		XStoreColor(display, colormap, &color);
		XSetForeground(display, solidbggc, currentbgcolor);
		XSetForeground(display, bgcolorgc, currentbgcolor);
		XSetForeground(display, backgroundgc, currentbgcolor);
	} else /* truecolor */ {
		currentbgcolor = paletteX11[VRAM[0x3f00] & 63];
		palette[24] = currentbgcolor;
		if (oldbgcolor != currentbgcolor) {
			XSetForeground(display, solidbggc, currentbgcolor);
			XSetForeground(display, bgcolorgc, currentbgcolor);
			XSetForeground(display, backgroundgc, currentbgcolor);
			renderer_data.redrawbackground = 1;
			renderer_data.needsredraw = 1;
		}
	}

	/* Tile colors */
	if (renderer_config.indexedcolor) {
		for (int x = 0; x < 24; x++) {
			if (VRAM[0x3f01 + x + (x / 3)] != palette_cache[0][1 + x + (x / 3)]) {
				color.pixel = colortableX11[x];
				color.red   = NES_palette[VRAM[0x3f01 + x + (x / 3)] & 63] >> 8 & 0xff00;
				color.green = NES_palette[VRAM[0x3f01 + x + (x / 3)] & 63]      & 0xff00;
				color.blue  = NES_palette[VRAM[0x3f01 + x + (x / 3)] & 63] << 8 & 0xff00;
				color.flags = DoRed | DoGreen | DoBlue;
				XStoreColor (display, colormap, &color);
				/*printf("color %d (%d) = %6x\n", x, colortableX11[x], paletteX11[VRAM[0x3f01+x+(x/3)]&63]); */
			}
		}
		memcpy(palette_cache[0], VRAM + 0x3f00, 32);
	}

	/* Set palette tables */
	if (renderer_config.indexedcolor) {
		/* Already done in InitDisplayX11 */
	} else /* truecolor */ {
		for (int x = 0; x < 24; x++) {
			palette[x] = paletteX11[VRAM[0x3f01 + x + (x / 3)] & 63];
		}
	}
}

#endif /* HAVE_X */
